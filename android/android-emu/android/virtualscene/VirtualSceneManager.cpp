/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "android/virtualscene/VirtualSceneManager.h"

#include "OpenGLESDispatch/GLESv2Dispatch.h"
#include "android/base/files/PathUtils.h"
#include "android/cmdline-option.h"
#include "android/featurecontrol/FeatureControl.h"
#include "android/globals.h"
#include "android/skin/winsys.h"
#include "android/utils/debug.h"
#include "android/virtualscene/Renderer.h"
#include "android/virtualscene/Scene.h"

#include <unordered_map>
#include <deque>

using namespace android::base;

#define E(...) derror(__VA_ARGS__)
#define W(...) dwarning(__VA_ARGS__)
#define D(...) VERBOSE_PRINT(virtualscene, __VA_ARGS__)
#define D_ACTIVE VERBOSE_CHECK(virtualscene)

namespace android {
namespace virtualscene {

LazyInstance<Lock> VirtualSceneManager::mLock = LAZY_INSTANCE_INIT;
Renderer* VirtualSceneManager::mRenderer = nullptr;
Scene* VirtualSceneManager::mScene = nullptr;

// Stores settings for the virtual scene.
//
// Access to the instance of this class, sSettings should be guarded by
// VirtualSceneManager::mLock.
class Settings {
public:
    Settings() = default;

    void parseCmdlineParameter(StringView param) {
        auto it = std::find(param.begin(), param.end(), '=');
        if (it == param.end()) {
            E("%s: Invalid command line parameter '%s', should be "
              "<name>=<filename>",
              __FUNCTION__, std::string(param).c_str());
            return;
        }

        std::string name(param.begin(), it);
        StringView filename(++it, param.end());

        std::string absFilename;
        if (!PathUtils::isAbsolute(filename)) {
            absFilename = PathUtils::join(System::get()->getCurrentDirectory(),
                                          filename);
        } else {
            absFilename = filename;
        }

        if (!System::get()->pathExists(absFilename)) {
            E("%s: Path '%s' does not exist.", __FUNCTION__,
              absFilename.c_str());
            return;
        }

        D("%s: Found poster %s at %s", __FUNCTION__, name.c_str(),
          absFilename.c_str());

        mPosters[name] = absFilename;
    }

    // Set the poster if it is not already defined.
    void setInitialPoster(const char* posterName, const char* filename) {
        if (mPosters.find(posterName) == mPosters.end()) {
            setPoster(posterName, filename);
        }
    }

    // Set the poster and queue a scene update for the next frame.
    void setPoster(const char* posterName, const char* filename) {
        mPosters[posterName] = filename;
        mPendingUpdates.push_back(posterName);
    }

    // Called when the scene is created, load the current poster configuration
    // in the scene.
    void setupScene(Scene* scene) {
        for (const auto& it : mPosters) {
            scene->loadPoster(it.first.c_str(), it.second.c_str());
        }

        mPendingUpdates.clear();
    }

    // Called each frame, apply any pending updates to the scene.  This must be
    // done on the OpenGL thread.
    void updateScene(Scene* scene) {
        while (!mPendingUpdates.empty()) {
            const std::string& posterName = mPendingUpdates.front();
            scene->loadPoster(posterName.c_str(), mPosters[posterName].c_str());

            mPendingUpdates.pop_front();
        }
    }

    const std::unordered_map<std::string, std::string>& getPosters() const {
        return mPosters;
    }

private:
    std::unordered_map<std::string, std::string> mPosters;
    std::deque<std::string> mPendingUpdates;
};

static LazyInstance<Settings> sSettings = LAZY_INSTANCE_INIT;

void VirtualSceneManager::parseCmdline() {
    AutoLock lock(mLock.get());
    if (sSettings.hasInstance()) {
        E("VirtualSceneManager settings already loaded");
        return;
    }

    if (!android_cmdLineOptions) {
        return;
    }

    if (!androidHwConfig_hasVirtualSceneCamera(android_hw) &&
        android_cmdLineOptions->virtualscene_poster) {
        W("[VirtualScene] Poster parameter ignored, virtual scene is not "
          "enabled.");
        return;
    }

    const ParamList* feature = android_cmdLineOptions->virtualscene_poster;
    while (feature) {
        sSettings->parseCmdlineParameter(feature->param);
        feature = feature->next;
    }
}

bool VirtualSceneManager::initialize(const GLESv2Dispatch* gles2,
                                     int width,
                                     int height) {
    AutoLock lock(mLock.get());
    if (mRenderer || mScene) {
        E("VirtualSceneManager already initialized");
        return false;
    }

    std::unique_ptr<Renderer> renderer = Renderer::create(gles2, width, height);
    if (!renderer) {
        E("VirtualSceneManager renderer failed to construct");
        return false;
    }

    std::unique_ptr<Scene> scene = Scene::create(*renderer.get());
    if (!scene) {
        E("VirtualSceneManager scene failed to load");
        return false;
    }

    sSettings->setupScene(scene.get());

    skin_winsys_show_virtual_scene_controls(true);

    // Store the raw pointers instead of the unique_ptr wrapper to prevent
    // unintented side-effects on process shutdown.
    mRenderer = renderer.release();
    mScene = scene.release();

    return true;
}

void VirtualSceneManager::uninitialize() {
    AutoLock lock(mLock.get());
    if (!mRenderer || !mScene) {
        E("VirtualSceneManager not initialized");
        return;
    }

    skin_winsys_show_virtual_scene_controls(false);

    mScene->releaseSceneObjects();
    delete mScene;
    mScene = nullptr;

    delete mRenderer;
    mRenderer = nullptr;
}

int64_t VirtualSceneManager::render() {
    AutoLock lock(mLock.get());
    if (!mRenderer || !mScene) {
        E("VirtualSceneManager not initialized");
        return 0L;
    }

    sSettings->updateScene(mScene);

    const int64_t timestamp = mScene->update();
    mRenderer->render(mScene->getRenderableObjects(),
            timestamp / 1000000000.0f);
    return timestamp;
}

void VirtualSceneManager::setInitialPoster(const char* posterName,
                                           const char* filename) {
    AutoLock lock(mLock.get());
    sSettings->setInitialPoster(posterName, filename);
}

bool VirtualSceneManager::loadPoster(const char* posterName,
                                     const char* filename) {
    AutoLock lock(mLock.get());
    sSettings->setPoster(posterName, filename);

    // If the scene is active, it will update the poster in the next render()
    // invocation.
    return true;
}

void VirtualSceneManager::enumeratePosters(void* context,
                                           EnumeratePostersCallback callback) {
    AutoLock lock(mLock.get());

    for (const auto& it : sSettings->getPosters()) {
        callback(context, it.first.c_str(),
                 it.second.empty() ? nullptr : it.second.c_str());
    }
}

}  // namespace virtualscene
}  // namespace android