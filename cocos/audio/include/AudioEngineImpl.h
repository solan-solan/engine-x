/****************************************************************************
 Copyright (c) 2014-2016 Chukong Technologies Inc.
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 Copyright (c) 2018-2020 HALX99.
 http://www.cocos2d-x.org

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/
#pragma once
#ifndef __AUDIO_ENGINE_IMPL_H_
#define __AUDIO_ENGINE_IMPL_H_

#include "platform/CCPlatformConfig.h"

#include <unordered_map>
#include <queue>

#include "base/CCRef.h"
#include "audio/include/AudioMacros.h"
#include "audio/include/AudioCache.h"
#include "audio/include/AudioPlayer.h"

NS_CC_BEGIN

class Scheduler;

class CC_DLL AudioEngineImpl : public cocos2d::Ref
{
public:
    AudioEngineImpl();
    ~AudioEngineImpl();

    bool init();
    AUDIO_ID play2d(const std::string &fileFullPath ,bool loop ,float volume);
    void setVolume(AUDIO_ID audioID,float volume);
    void setLoop(AUDIO_ID audioID, bool loop);
    bool pause(AUDIO_ID audioID);
    bool resume(AUDIO_ID audioID);
    void stop(AUDIO_ID audioID);
    void stopAll();
    float getDuration(AUDIO_ID audioID);
    float getCurrentTime(AUDIO_ID audioID);
    bool setCurrentTime(AUDIO_ID audioID, float time);
    void setFinishCallback(AUDIO_ID audioID, const std::function<void (AUDIO_ID, const std::string &)> &callback);

    void uncache(const std::string& filePath);
    void uncacheAll();
    AudioCache* preload(const std::string& filePath, std::function<void(bool)> callback);
    void update(float dt);

private:
    void _updateLocked(float dt);
    void _play2d(AudioCache *cache, AUDIO_ID audioID);
    ALuint findValidSource();
#if !CC_USE_ALSOFT
    static ALvoid myAlSourceNotificationCallback(ALuint sid, ALuint notificationID, ALvoid* userData);
#endif
    ALuint _alSources[MAX_AUDIOINSTANCES];

    //available sources
    std::queue<ALuint> _unusedSourcesPool;

    //filePath,bufferInfo
    std::unordered_map<std::string, AudioCache> _audioCaches;

    //audioID,AudioInfo
    std::unordered_map<AUDIO_ID, AudioPlayer*>  _audioPlayers;
    std::recursive_mutex _threadMutex;

    //finish callbacks
    std::vector<std::function<void()>> _finishCallbacks;

    bool _lazyInitLoop;

    AUDIO_ID _currentAudioID;
    Scheduler* _scheduler;
};

NS_CC_END
#endif // __AUDIO_ENGINE_INL_H_

