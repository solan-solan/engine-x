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
#define LOG_TAG "AudioEngineImpl"

#include "platform/CCPlatformConfig.h"

#include "audio/include/AudioEngineImpl.h"

#ifdef OPENAL_PLAIN_INCLUDES
#include "alc.h"
#include "alext.h"
#else
#include "AL/alc.h"
#include "AL/alext.h"
#endif
#include "audio/include/AudioEngine.h"
#include "base/CCDirector.h"
#include "base/CCScheduler.h"
#include "platform/CCFileUtils.h"
#include "audio/include/AudioDecoderManager.h"
#include "audio/include/AudioPlayer.h"

using namespace cocos2d;

static ALCdevice *s_ALDevice = nullptr;
static ALCcontext *s_ALContext = nullptr;

AudioEngineImpl::AudioEngineImpl()
: _lazyInitLoop(true)
, _currentAudioID(0)
, _scheduler(nullptr)
{

}

AudioEngineImpl::~AudioEngineImpl()
{
    if (_scheduler != nullptr)
    {
        _scheduler->unschedule(CC_SCHEDULE_SELECTOR(AudioEngineImpl::update), this);
    }

    if (s_ALContext) {
        alDeleteSources(MAX_AUDIOINSTANCES, _alSources);

        _audioCaches.clear();

        alcMakeContextCurrent(nullptr);
        alcDestroyContext(s_ALContext);
        s_ALContext = nullptr;
    }

    if (s_ALDevice) {
        alcCloseDevice(s_ALDevice);
        s_ALDevice = nullptr;
    }

    AudioDecoderManager::destroy();
}

bool AudioEngineImpl::init()
{
    bool ret = false;
    do{
        s_ALDevice = alcOpenDevice(nullptr);

        if (s_ALDevice) {
            alGetError();
            s_ALContext = alcCreateContext(s_ALDevice, nullptr);
            alcMakeContextCurrent(s_ALContext);

            alGenSources(MAX_AUDIOINSTANCES, _alSources);
            auto alError = alGetError();
            if(alError != AL_NO_ERROR)
            {
                ALOGE("%s:generating sources failed! error = %x\n", __FUNCTION__, alError);
                break;
            }

            for (int i = 0; i < MAX_AUDIOINSTANCES; ++i) {
                _unusedSourcesPool.push(_alSources[i]);
            }

            _scheduler = Director::getInstance()->getScheduler();
            ret = AudioDecoderManager::init();
            ALOGI("OpenAL was initialized successfully!");
        }
    }while (false);

    return ret;
}

AudioCache* AudioEngineImpl::preload(const std::string& filePath, std::function<void(bool)> callback)
{
    AudioCache* audioCache = nullptr;

    auto it = _audioCaches.find(filePath);
    if (it == _audioCaches.end()) {
        audioCache = &_audioCaches[filePath];
        audioCache->_fileFullPath = FileUtils::getInstance()->fullPathForFilename(filePath);
        unsigned int cacheId = audioCache->_id;
        auto isCacheDestroyed = audioCache->_isDestroyed;
        AudioEngine::addTask([audioCache, cacheId, isCacheDestroyed](){
            if (*isCacheDestroyed)
            {
                ALOGV("AudioCache (id=%u) was destroyed, no need to launch readDataTask.", cacheId);
                audioCache->setSkipReadDataTask(true);
                return;
            }
            audioCache->readDataTask(cacheId);
        });
    }
    else {
        audioCache = &it->second;
    }

    if (audioCache && callback)
    {
        audioCache->addLoadCallback(callback);
    }
    return audioCache;
}

AUDIO_ID AudioEngineImpl::play2d(const std::string &filePath ,bool loop ,float volume)
{
    if (s_ALDevice == nullptr) {
        return AudioEngine::INVALID_AUDIO_ID;
    }

    ALuint alSource = findValidSource();
    if (alSource == AL_INVALID)
    {
        return AudioEngine::INVALID_AUDIO_ID;
    }

    auto player = new (std::nothrow) AudioPlayer;
    if (player == nullptr) {
        return AudioEngine::INVALID_AUDIO_ID;
    }

    player->_alSource = alSource;
    player->_loop = loop;
    player->_volume = volume;

    auto audioCache = preload(filePath, nullptr);
    if (audioCache == nullptr) {
        delete player;
        return AudioEngine::INVALID_AUDIO_ID;
    }

    player->setCache(audioCache);
    _threadMutex.lock();
    _audioPlayers.emplace(++_currentAudioID, player);
    _threadMutex.unlock();

    audioCache->addPlayCallback(std::bind(&AudioEngineImpl::_play2d,this,audioCache,_currentAudioID));

    if (_lazyInitLoop) {
        _lazyInitLoop = false;
        _scheduler->schedule(CC_SCHEDULE_SELECTOR(AudioEngineImpl::update), this, 0.05f, false);
    }

    return _currentAudioID;
}

void AudioEngineImpl::_play2d(AudioCache *cache, AUDIO_ID audioID)
{
    std::unique_lock<std::recursive_mutex> lck(_threadMutex);
    auto iter = _audioPlayers.find(audioID);
    if (iter == _audioPlayers.end())
        return;
    auto player = iter->second;

    //Note: It maybe in sub thread or main thread :(
    if (!*cache->_isDestroyed && cache->_state == AudioCache::State::READY)
    {
        if (player->play2d()) {
            _scheduler->performFunctionInCocosThread([audioID](){

                if (AudioEngine::_audioIDInfoMap.find(audioID) != AudioEngine::_audioIDInfoMap.end()) {
                    AudioEngine::_audioIDInfoMap[audioID].state = AudioEngine::AudioState::PLAYING;
                }
            });
        }
    }
    else
    {
        ALOGD("AudioEngineImpl::_play2d, cache was destroyed or not ready!");
        player->_removeByAudioEngine = true;
    }
}

ALuint AudioEngineImpl::findValidSource()
{
    ALuint sourceId = AL_INVALID;
    if (!_unusedSourcesPool.empty())
    {
        sourceId = _unusedSourcesPool.front();
        _unusedSourcesPool.pop();
    }

    return sourceId;
}

void AudioEngineImpl::setVolume(AUDIO_ID audioID,float volume)
{
    std::unique_lock<std::recursive_mutex> lck(_threadMutex);
    auto iter = _audioPlayers.find(audioID);
    if(iter == _audioPlayers.end())
        return;
    
    auto player = iter->second;
    lck.unlock();
    
    player->_volume = volume;

    if (player->_ready) {
        alSourcef(player->_alSource, AL_GAIN, volume);

        auto error = alGetError();
        if (error != AL_NO_ERROR) {
            ALOGE("%s: audio id = " AUDIO_ID_PRID ", error = %x", __FUNCTION__, audioID, error);
        }
    }
}

void AudioEngineImpl::setLoop(AUDIO_ID audioID, bool loop)
{
    std::unique_lock<std::recursive_mutex> lck(_threadMutex);
    auto iter = _audioPlayers.find(audioID);
    if(iter == _audioPlayers.end())
        return;
    
    auto player = iter->second;

    lck.unlock();
    
    if (player->_ready) {
        if (player->_streamingSource) {
            player->setLoop(loop);
        } else {
            if (loop) {
                alSourcei(player->_alSource, AL_LOOPING, AL_TRUE);
            } else {
                alSourcei(player->_alSource, AL_LOOPING, AL_FALSE);
            }

            auto error = alGetError();
            if (error != AL_NO_ERROR) {
                ALOGE("%s: audio id = " AUDIO_ID_PRID ", error = %x", __FUNCTION__, audioID, error);
            }
        }
    }
    else {
        player->_loop = loop;
    }
}

bool AudioEngineImpl::pause(AUDIO_ID audioID)
{
    std::unique_lock<std::recursive_mutex> lck(_threadMutex);
    auto iter = _audioPlayers.find(audioID);
    if(iter == _audioPlayers.end())
        return false;
    
    auto player = iter->second;
    
    lck.unlock();
    
    bool ret = true;
    alSourcePause(player->_alSource);

    auto error = alGetError();
    if (error != AL_NO_ERROR) {
        ret = false;
        ALOGE("%s: audio id = " AUDIO_ID_PRID ", error = %x\n", __FUNCTION__, audioID, error);
    }

    return ret;
}

bool AudioEngineImpl::resume(AUDIO_ID audioID)
{
    bool ret = true;
    std::unique_lock<std::recursive_mutex> lck(_threadMutex);
    auto iter = _audioPlayers.find(audioID);
    if(iter == _audioPlayers.end())
        return false;
    
    auto player = iter->second;
    lck.unlock();
    
    alSourcePlay(player->_alSource);

    auto error = alGetError();
    if (error != AL_NO_ERROR) {
        ret = false;
        ALOGE("%s: audio id = " AUDIO_ID_PRID ", error = %x\n", __FUNCTION__, audioID, error);
    }

    return ret;
}

void AudioEngineImpl::stop(AUDIO_ID audioID)
{
    std::unique_lock<std::recursive_mutex> lck(_threadMutex);
    auto iter = _audioPlayers.find(audioID);
    if(iter == _audioPlayers.end())
        return;
    
    auto player = iter->second;

    player->destroy();

    // Call 'update' method to cleanup immediately since the schedule may be cancelled without any notification.
    _updateLocked(0.0f);
}

void AudioEngineImpl::stopAll()
{
    std::lock_guard<std::recursive_mutex> lck(_threadMutex);
    for(auto&& player : _audioPlayers)
    {
        player.second->destroy();
    }
    //Note: Don't set the flag to false here, it should be set in 'update' function.
    // Otherwise, the state got from alSourceState may be wrong
//    for(int index = 0; index < MAX_AUDIOINSTANCES; ++index)
//    {
//        _alSourceUsed[_alSources[index]] = false;
//    }

    // Call 'update' method to cleanup immediately since the schedule may be cancelled without any notification.
    _updateLocked(0.0f);
}

float AudioEngineImpl::getDuration(AUDIO_ID audioID)
{
    std::lock_guard<std::recursive_mutex> lck(_threadMutex);
    auto it = _audioPlayers.find(audioID);
    if (it != _audioPlayers.end()) {
        auto player = it->second;
        if (player->_ready) {
            return player->_audioCache->_duration;
        }
    }
    return AudioEngine::TIME_UNKNOWN;
}

float AudioEngineImpl::getCurrentTime(AUDIO_ID audioID)
{
    std::unique_lock<std::recursive_mutex> lck(_threadMutex);
    auto it = _audioPlayers.find(audioID); 
    if (it == _audioPlayers.end())
        return 0.0f;

    float ret = 0.0f;
    auto player = it->second;
    if (player->_ready) {
        if (player->_streamingSource) {
            ret = player->getTime();
        }
        else {
            alGetSourcef(player->_alSource, AL_SEC_OFFSET, &ret);

            auto error = alGetError();
            if (error != AL_NO_ERROR) {
                ALOGE("%s, audio id:" AUDIO_ID_PRID ",error code:%x", __FUNCTION__, audioID, error);
            }
        }
    }

    return ret;
}

bool AudioEngineImpl::setCurrentTime(AUDIO_ID audioID, float time)
{
    bool ret = false;
    std::unique_lock<std::recursive_mutex> lck(_threadMutex);
    auto iter = _audioPlayers.find(audioID);
    if(iter == _audioPlayers.end())
        return false;
    
    auto player = iter->second;

    do {
        if (!player->_ready) {
            break;
        }

        if (player->_streamingSource) {
            ret = player->setTime(time);
            break;
        }
        else {
            if (player->_audioCache->_framesRead != player->_audioCache->_totalFrames &&
                (time * player->_audioCache->_sampleRate) > player->_audioCache->_framesRead) {
                ALOGE("%s: audio id = " AUDIO_ID_PRID, __FUNCTION__, audioID);
                break;
            }

            alSourcef(player->_alSource, AL_SEC_OFFSET, time);

            auto error = alGetError();
            if (error != AL_NO_ERROR) {
                ALOGE("%s: audio id = " AUDIO_ID_PRID ", error = %x", __FUNCTION__, audioID, error);
            }
            ret = true;
        }
    } while (0);

    return ret;
}

void AudioEngineImpl::setFinishCallback(AUDIO_ID audioID, const std::function<void (AUDIO_ID, const std::string &)> &callback)
{
    std::unique_lock<std::recursive_mutex> lck(_threadMutex);
    auto iter = _audioPlayers.find(audioID);
    if(iter == _audioPlayers.end())
        return;
    
    auto player = iter->second;
    lck.unlock();
    
    player->_finishCallbak = callback;
}

void AudioEngineImpl::update(float dt)
{
    std::unique_lock<std::recursive_mutex> lck(_threadMutex);
    _updateLocked(dt);
}

void AudioEngineImpl::_updateLocked(float dt)
{
    AUDIO_ID audioID;
    AudioPlayer* player;
    ALuint alSource;

//    ALOGV("AudioPlayer count: %d", (int)_audioPlayers.size());
    for (auto it = _audioPlayers.begin(); it != _audioPlayers.end(); ) {
        audioID = it->first;
        player = it->second;
        alSource = player->_alSource;

        if (player->_removeByAudioEngine)
        {
            AudioEngine::remove(audioID);
            
            it = _audioPlayers.erase(it);
            delete player;
            _unusedSourcesPool.push(alSource);
        }
        else if (player->_ready && player->isFinished()) {

            std::string filePath;
            if (player->_finishCallbak) {
                auto& audioInfo = AudioEngine::_audioIDInfoMap[audioID];
                filePath = audioInfo.filePath;
            }

            AudioEngine::remove(audioID);
            
            it = _audioPlayers.erase(it);

            if (player->_finishCallbak) {
                /// ###IMPORTANT: don't call immidiately, because at callback, user-end may play a new audio
                /// cause _audioPlayers' iterator goan to invalid.
                _finishCallbacks.push_back([finishCallback = std::move(player->_finishCallbak), audioID, filePath = std::move(filePath)](){
                    finishCallback(audioID, filePath); // FIXME: callback will delay 50ms
                });
            }
            // clear cache when audio player finsihed properly
            player->setCache(nullptr);
            delete player;
            _unusedSourcesPool.push(alSource);
        }
        else{
            ++it;
        }
    }

    if(_audioPlayers.empty()) {
        _lazyInitLoop = true;
        _scheduler->unschedule(CC_SCHEDULE_SELECTOR(AudioEngineImpl::update), this);
    }
    if (!_finishCallbacks.empty()) {
        for (auto& finishCallback : _finishCallbacks)
            finishCallback();
        _finishCallbacks.clear();
    }
}

void AudioEngineImpl::uncache(const std::string &filePath)
{
    _audioCaches.erase(filePath);
}

void AudioEngineImpl::uncacheAll()
{
    // prevent player hold invalid AudioCache* pointer, since all audio caches purged
    for (auto& player : _audioPlayers)
        player.second->setCache(nullptr);

    _audioCaches.clear();
}

