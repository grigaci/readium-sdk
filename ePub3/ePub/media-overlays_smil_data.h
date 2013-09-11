//
//  media-overlays_smil_data.h
//  ePub3
//
//  Created by Daniel Weck on 2013-09-18.
//  Copyright (c) 2012-2013 The Readium Foundation and contributors.
//  
//  The Readium SDK is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//  
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//  
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef ePub3_mo_smil_data_h
#define ePub3_mo_smil_data_h

#include <ePub3/epub3.h>
#include <ePub3/utilities/owned_by.h>
#include <ePub3/manifest.h>
#include <ePub3/spine.h>

EPUB3_BEGIN_NAMESPACE

        class MediaOverlaysSmilModel;

        class SMILData;

        class SMILData : public OwnedBy<MediaOverlaysSmilModel> //public std::enable_shared_from_this<SMILData>
        {
            friend class MediaOverlaysSmilModel; // _root

        public:

            class Sequence;

            class Parallel;

            class TimeContainer;

            class Audio;

            class Text;

            class TimeNode : public OwnedBy<SMILData> 
            {
            private:
                TimeNode() _DELETED_;

                TimeNode(const TimeNode &) _DELETED_;

                TimeNode(TimeNode &&) _DELETED_;

            protected:
                TimeContainer * _parent;

            public:
                virtual const string & Name() const;

                virtual ~TimeNode();

                EPUB3_EXPORT

                TimeNode(TimeContainer * parent, const std::shared_ptr<SMILData> & smilData) : _parent(parent), OwnedBy(smilData)
                {
                }

                EPUB3_EXPORT

                const TimeContainer * Parent() const
                {
                    return _parent;
                }
            };

            class TimeContainer : public TimeNode
            {
            private:
                TimeContainer() _DELETED_;

                TimeContainer(const TimeContainer &) _DELETED_;

                TimeContainer(TimeContainer &&) _DELETED_;

            protected:
                string _type; // space-separated

            public:
                virtual ~TimeContainer();

                EPUB3_EXPORT

                TimeContainer(Sequence * parent, string type, const std::shared_ptr<SMILData> & smilData) : TimeNode(parent, smilData), _type(type)
                {
                    if (parent != nullptr)
                    {
                        parent->_children.push_back(this);
                    }
                }

                EPUB3_EXPORT

                const Sequence * ParentSequence() const
                {
                    return dynamic_cast<const Sequence *>(_parent);
                }

                EPUB3_EXPORT

                const string Type() const
                {
                    return _type;
                }

                EPUB3_EXPORT

                virtual const bool IsParallel() const;

                EPUB3_EXPORT

                virtual const bool IsSequence() const;
            };

            class Sequence : public TimeContainer
            {
                friend class TimeContainer; // _children

                friend class SMILData; // ParallelAt, NthParallel, ClipOffset

            private:
                Sequence() _DELETED_;

                Sequence(const Sequence &) _DELETED_;

                Sequence(Sequence &&) _DELETED_;

                static const string _Name;

            protected:
                string _textref_file;
                string _textref_fragmentID;

                const std::shared_ptr<ManifestItem> _textrefManifestItem;

                std::vector<const TimeContainer *> _children;

                const bool ClipOffset(uint32_t & offset, const Parallel * par) const
                {
                    std::shared_ptr<SMILData> smilData = Owner(); // internally: std::weak_ptr<SMILData>.lock()
                    if (smilData == nullptr)
                    {
                        return false;
                    }
                    
                    for (int i = 0; i < _children.size(); i++)
                    {
                        const TimeContainer * container = _children[i];
                        if (container->IsParallel())
                        {
                            const Parallel * para = dynamic_cast<const Parallel *>(container);
                            if (para == par)
                            {
                                return true;
                            }

                            if (para->Audio() == nullptr)
                            {
                                continue;
                            }

                            if (para->Text() != nullptr && para->Text()->SrcManifestItem() != nullptr && para->Text()->SrcManifestItem() != smilData->XhtmlSpineItem()->ManifestItem())
                            {
                                continue;
                            }

                            uint32_t clipDur = para->Audio()->ClipDurationMilliseconds();
                            offset += clipDur;
                        }
                        else if (container->IsSequence())
                        {
                            const Sequence * sequence = dynamic_cast<const Sequence *>(container);

                            bool found = sequence->ClipOffset(offset, par);
                            if (found)
                            {
                                return true;
                            }
                        }
                    }

                    return false;
                }

                const Parallel * ParallelAt(uint32_t timeMilliseconds) const
                {
                    std::shared_ptr<SMILData> smilData = Owner(); // internally: std::weak_ptr<SMILData>.lock()
                    if (smilData == nullptr)
                    {
                        return nullptr;
                    }
                    
                    uint32_t offset = 0;

                    for (int i = 0; i < _children.size(); i++)
                    {
                        uint32_t timeAdjusted = timeMilliseconds - offset;

                        const TimeContainer * container = _children[i];
                        if (container->IsParallel())
                        {
                            const Parallel * para = dynamic_cast<const Parallel *>(container);

                            if (para->Audio() == nullptr)
                            {
                                continue;
                            }

                            if (para->Text() != nullptr && para->Text()->SrcManifestItem() != nullptr && para->Text()->SrcManifestItem() != smilData->XhtmlSpineItem()->ManifestItem())
                            {
                                continue;
                            }

                            uint32_t clipDur = para->Audio()->ClipDurationMilliseconds();

                            if (clipDur > 0 && timeAdjusted <= clipDur)
                            {
                                return para;
                            }

                            offset += clipDur;
                        }
                        else if (container->IsSequence())
                        {
                            const Sequence * sequence = dynamic_cast<const Sequence *>(container);

                            const Parallel * para = sequence->ParallelAt(timeAdjusted);
                            if (para != nullptr)
                            {
                                return para;
                            }

                            offset += sequence->DurationMilliseconds();
                        }
                    }

                    return nullptr;
                }

                const Parallel * NthParallel(uint32_t index, uint32_t & count) const
                {
                    for (int i = 0; i < _children.size(); i++)
                    {
                        const TimeContainer * container = _children[i];
                        if (container->IsParallel())
                        {
                            count++;

                            if (count == index)
                            {
                                const Parallel * para = dynamic_cast<const Parallel *>(container);
                                return para;
                            }
                        }
                        else if (container->IsSequence())
                        {
                            const Sequence * sequence = dynamic_cast<const Sequence *>(container);

                            const Parallel * para = sequence->NthParallel(index, count);
                            if (para != nullptr)
                            {
                                return para;
                            }
                        }
                    }

                    return nullptr;
                }

            public:
                EPUB3_EXPORT

                const string & Name() const
                {
                    return _Name;
                }

                virtual ~Sequence();

                EPUB3_EXPORT

                Sequence(Sequence * parent, string textref_file, string textref_fragmentID, const std::shared_ptr<ManifestItem> textrefManifestItem, string type, const std::shared_ptr<SMILData> & smilData) : TimeContainer(parent, type, smilData), _textref_file(textref_file), _textref_fragmentID(textref_fragmentID), _textrefManifestItem(textrefManifestItem)
                {
                    _children = std::vector<const TimeContainer *>();
                }

                EPUB3_EXPORT

                const string TextRefFile() const
                {
                    return _textref_file;
                }

                EPUB3_EXPORT

                const string TextRefFragmentId() const
                {
                    return _textref_fragmentID;
                }

                EPUB3_EXPORT

                const std::shared_ptr<ManifestItem> TextRefManifestItem() const
                {
                    return _textrefManifestItem;
                }

                EPUB3_EXPORT

                const std::vector<const TimeContainer *>::size_type GetChildrenCount() const
                {
                    return _children.size();
                }

                EPUB3_EXPORT

                const TimeContainer * GetChild(std::vector<const TimeContainer *>::size_type i) const
                {
                    if (i >= _children.size())
                    {
                        return nullptr;
                    }

                    return _children[i];
                }

                EPUB3_EXPORT

                const bool IsParallel() const
                {
                    return false;
                }

                EPUB3_EXPORT

                const bool IsSequence() const
                {
                    return true;
                }

                EPUB3_EXPORT

                const uint32_t DurationMilliseconds() const
                {
                    std::shared_ptr<SMILData> smilData = Owner(); // internally: std::weak_ptr<SMILData>.lock()
                    if (smilData == nullptr)
                    {
                        return 0;
                    }
                    
                    uint32_t total = 0;

                    for (int i = 0; i < _children.size(); i++)
                    {
                        const TimeContainer * container = _children[i];
                        if (container->IsParallel())
                        {
                            const Parallel * para = dynamic_cast<const Parallel *>(container);

                            if (para->Audio() == nullptr)
                            {
                                continue;
                            }

                            if (para->Text() != nullptr && para->Text()->SrcManifestItem() != nullptr && para->Text()->SrcManifestItem() != smilData->XhtmlSpineItem()->ManifestItem())
                            {
                                continue;
                            }

                            uint32_t clipDur = para->Audio()->ClipDurationMilliseconds();
                            total += clipDur;
                        }
                        else if (container->IsSequence())
                        {
                            const Sequence * sequence = dynamic_cast<const Sequence *>(container);

                            total += sequence->DurationMilliseconds();
                        }
                    }

                    return total;
                }
            };

            class Media : public TimeNode
            {
            private:
                Media() _DELETED_;

                Media(const Media &) _DELETED_;

                Media(Media &&) _DELETED_;

            protected:
                string _src_file;
                string _src_fragmentID;
                const std::shared_ptr<ManifestItem> _srcManifestItem;

            public:
                virtual ~Media();

                EPUB3_EXPORT

                Media(Parallel * parent, string src_file, string src_fragmentID, const std::shared_ptr<ManifestItem> srcManifestItem, const std::shared_ptr<SMILData> & smilData) : TimeNode(parent, smilData), _src_file(src_file), _src_fragmentID(src_fragmentID), _srcManifestItem(srcManifestItem)
                {
                }

                EPUB3_EXPORT

                const Parallel * ParentParallel() const
                {
                    return dynamic_cast<const Parallel *>(_parent);
                }

                EPUB3_EXPORT

                const string SrcFile() const
                {
                    return _src_file;
                }

                EPUB3_EXPORT

                const string SrcFragmentId() const
                {
                    return _src_fragmentID;
                }

                EPUB3_EXPORT

                const std::shared_ptr<ManifestItem> SrcManifestItem() const
                {
                    return _srcManifestItem;
                }

                EPUB3_EXPORT

                virtual const bool IsAudio() const;

                EPUB3_EXPORT

                virtual const bool IsText() const;
            };

            class Audio : public Media
            {
            private:
                Audio() _DELETED_;

                Audio(const Audio &) _DELETED_;

                Audio(Audio &&) _DELETED_;

                static const string _Name;

            protected:
                uint32_t _clipBeginMilliseconds;
                uint32_t _clipEndMilliseconds;

            public:
                EPUB3_EXPORT

                const string & Name() const
                {
                    return _Name;
                }

                virtual ~Audio();

                EPUB3_EXPORT

                Audio(Parallel * parent, string src, const std::shared_ptr<ManifestItem> srcManifestItem, uint32_t clipBeginMilliseconds, uint32_t clipEndMilliseconds, const std::shared_ptr<SMILData> & smilData) : Media(parent, src, "", srcManifestItem, smilData), _clipBeginMilliseconds(clipBeginMilliseconds), _clipEndMilliseconds(clipEndMilliseconds)
                {
                    parent->_audio = this;
                }

                EPUB3_EXPORT

                const uint32_t ClipBeginMilliseconds() const
                {
                    return _clipBeginMilliseconds;
                }

                EPUB3_EXPORT

                const uint32_t ClipEndMilliseconds() const
                {
                    return _clipEndMilliseconds;
                }

                EPUB3_EXPORT

                const uint32_t ClipDurationMilliseconds() const
                {
                    if (_clipEndMilliseconds <= 0 || _clipEndMilliseconds <= _clipBeginMilliseconds)
                    {
                        return 0;
                    }

                    return _clipEndMilliseconds - _clipBeginMilliseconds;
                }

                EPUB3_EXPORT

                const bool IsAudio() const
                {
                    return true;
                }

                EPUB3_EXPORT

                const bool IsText() const
                {
                    return false;
                }
            };

            class Text : public Media
            {
            private:
                Text() _DELETED_;

                Text(const Text &) _DELETED_;

                Text(Text &&) _DELETED_;

                static const string _Name;

            public:
                EPUB3_EXPORT

                const string & Name() const
                {
                    return _Name;
                }

                virtual ~Text();

                EPUB3_EXPORT

                Text(Parallel * parent, string src_file, string src_fragmentID, const std::shared_ptr<ManifestItem> srcManifestItem, const std::shared_ptr<SMILData> & smilData) : Media(parent, src_file, src_fragmentID, srcManifestItem, smilData)
                {
                    parent->_text = this;
                }

                EPUB3_EXPORT

                const bool IsAudio() const
                {
                    return false;
                }

                EPUB3_EXPORT

                const bool IsText() const
                {
                    return true;
                }
            };

            class Parallel : public TimeContainer
            {
                friend class Text; // _text

                friend class Audio; // _audio

            private:
                Parallel() _DELETED_;

                Parallel(const Parallel &) _DELETED_;

                Parallel(Parallel &&) _DELETED_;

                static const string _Name;

            protected:
                Audio * _audio;
                Text * _text;

            public:

                EPUB3_EXPORT

                const string & Name() const
                {
                    return _Name;
                }

                virtual ~Parallel();

                EPUB3_EXPORT

                Parallel(Sequence * parent, string type, const std::shared_ptr<SMILData> & smilData):TimeContainer(parent, type, smilData), _audio(nullptr), _text(nullptr)
                {
                }

                EPUB3_EXPORT

                const Audio * Audio() const
                {
                    return _audio;
                }

                EPUB3_EXPORT

                const Text * Text() const
                {
                    return _text;
                }

                EPUB3_EXPORT

                const bool IsParallel() const
                {
                    return true;
                }

                EPUB3_EXPORT

                const bool IsSequence() const
                {
                    return false;
                }
            };

        private :
            SMILData() _DELETED_;

            SMILData(const SMILData &) _DELETED_;

            SMILData(SMILData &&) _DELETED_;

        protected:
            uint32_t _duration; //whole milliseconds (resolution = 1ms)

            const std::shared_ptr<ManifestItem> _manifestItem;

            const std::shared_ptr<SpineItem> _spineItem;

            Sequence * _root;

            const Parallel * ParallelAt(uint32_t timeMilliseconds) const
            {
                if (_root == nullptr)
                {
                    return nullptr;
                }

                return _root->ParallelAt(timeMilliseconds);
            }

            const Parallel * NthParallel(uint32_t index) const
            {
                if (_root == nullptr)
                {
                    return nullptr;
                }

                uint32_t count = -1;
                return _root->NthParallel(index, count);
            }

            const uint32_t ClipOffset(const Parallel * par) const
            {
                if (_root == nullptr)
                {
                    return 0;
                }

                uint32_t offset = 0;
                if (_root->ClipOffset(offset, par))
                {
                    return offset;
                }

                return 0;
            }

        public:
            EPUB3_EXPORT

            SMILData(const std::shared_ptr<MediaOverlaysSmilModel> & smilModel, const std::shared_ptr<ManifestItem> manifestItem, const std::shared_ptr<SpineItem> spineItem, uint32_t duration) : OwnedBy(smilModel), _manifestItem(manifestItem), _spineItem(spineItem), _duration(duration), _root(nullptr)
            {
                //printf("SMILData(%s)\n", manifestItem->Href().c_str());
            }

            virtual ~SMILData();

            EPUB3_EXPORT

            const std::shared_ptr<SpineItem> XhtmlSpineItem() const
            {
                return _spineItem;
            }

            EPUB3_EXPORT

            const std::shared_ptr<ManifestItem> SmilManifestItem() const
            {
                return _manifestItem;
            }

            EPUB3_EXPORT

            const Sequence * Body() const
            {
                if (_root == nullptr)
                {
                    return nullptr;
                }
                
                //return _root.get();
                return _root;
            }

            EPUB3_EXPORT

            const uint32_t DurationMilliseconds_Metadata() const
            {
                return _duration;
            }

            EPUB3_EXPORT

            const uint32_t DurationMilliseconds_Calculated() const
            {
                if (_root == nullptr)
                {
                    return 0;
                }

                return _root->DurationMilliseconds();
            }
        };

        EPUB3_END_NAMESPACE

#endif /* defined(ePub3_mo_smil_data_h) */