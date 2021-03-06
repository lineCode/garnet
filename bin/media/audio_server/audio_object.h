// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_BIN_MEDIA_AUDIO_SERVER_AUDIO_OBJECT_H_
#define GARNET_BIN_MEDIA_AUDIO_SERVER_AUDIO_OBJECT_H_

#include <fbl/auto_lock.h>
#include <fbl/intrusive_double_list.h>
#include <fbl/mutex.h>
#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>

#include "garnet/bin/media/audio_server/fwd_decls.h"
#include "lib/fxl/synchronization/thread_annotations.h"

namespace media {
namespace audio {

// An audio object is the simple base class for 4 major types of audio objects
// in the mixer; Outputs, Inputs, Renderers and Capturers.  It ensures that each
// of these objects is intrusively ref-counted, and remembers its type so that
// it may be safely downcast from a generic audio object to something more
// specific.
//
// TODO(johngro) : Refactor AudioRenderers so that they derive from AudioObject.
class AudioObject : public fbl::RefCounted<AudioObject> {
 private:
  using NodeState = fbl::DoublyLinkedListNodeState<fbl::RefPtr<AudioObject>>;
  struct ListTraits {
    static NodeState& node_state(AudioObject& obj) { return obj.node_state_; }
  };

 public:
  using List = fbl::DoublyLinkedList<fbl::RefPtr<AudioObject>, ListTraits>;
  enum class Type {
    Output,
    Input,
    Renderer,
    Capturer,
  };

  static std::shared_ptr<AudioLink> LinkObjects(
      const fbl::RefPtr<AudioObject>& source,
      const fbl::RefPtr<AudioObject>& dest);
  static void RemoveLink(const AudioLinkPtr& link);

  void UnlinkSources();
  void UnlinkDestinations();
  void Unlink() {
    UnlinkSources();
    UnlinkDestinations();
  }

  // PreventNewLinks
  //
  // Clears the new_links_allowed flag from within the context of the
  // links_lock.  This ensures that no new links may be added to this object
  // anymore.  Calling PreventNewLinks is one of the first steps in the process
  // of shutting down an AudioObject.
  //
  // TODO(johngro) : Consider eliminating this; given the way that links are
  // created and destroyed, it is not clear if it is needed anymore.
  void PreventNewLinks() {
    fbl::AutoLock lock(&links_lock_);
    new_links_allowed_ = false;
  }

  Type type() const { return type_; }
  bool is_output() const { return type() == Type::Output; }
  bool is_input() const { return type() == Type::Input; }
  bool is_renderer() const { return type() == Type::Renderer; }
  bool is_capturer() const { return type() == Type::Capturer; }

  bool in_object_list() const { return node_state_.InContainer(); }

 protected:
  friend class fbl::RefPtr<AudioObject>;
  explicit AudioObject(Type type) : type_(type) {}
  virtual ~AudioObject() {}

  // Initialize(Source|Dest)Link
  //
  // Called on the AudioServer's main message loop any time a source and a
  // destination are being linked via AudioObject::LinkObjects.  By default,
  // these hooks do nothing, but AudioObject subclasses may use them to set the
  // properties of a link (or reject the link) before the link gets added to the
  // source and destination link sets.
  //
  // For example, Sources like a renderer override InitializeDestLink in order
  // to set the source gain and to make a copy of their pending packet
  // queue.packet queue.  Destinations like an output override
  // InitializeSourceLink in order to choose and intialize an appropriate
  // resampling filter.
  //
  // @return MediaResult::OK if initialization succeeded, or an appropriate
  // error code otherwise.
  virtual zx_status_t InitializeSourceLink(const AudioLinkPtr& link);
  virtual zx_status_t InitializeDestLink(const AudioLinkPtr& link);

  fbl::Mutex links_lock_;

  // The set of links which this audio device is acting as a source for (eg; the
  // destinations that this object is sending to).  The target of each of these
  // links must be a either an Output or a Capturer.
  AudioLinkSet dest_links_ FXL_GUARDED_BY(links_lock_);

  // The set of links which this audio device is acting as a destination for
  // (eg; the sources that that the object is receiving from).  The target of
  // each of these links must be a either an Output or a Capturer.
  //
  // TODO(johngro): Order this by priority.  Use a fbl::WAVLTree (or some other
  // form of ordered intrusive container) so that we can easily remove and
  // re-insert a link if/when priority changes.
  //
  // Right now, we have no priorities, so this is just a set of renderer/output
  // links.
  AudioLinkSet source_links_ FXL_GUARDED_BY(links_lock_);

 private:
  void UnlinkCleanup(AudioLinkSet* links);

  const Type type_;
  bool new_links_allowed_ FXL_GUARDED_BY(links_lock_) = true;
  NodeState node_state_;
};

}  // namespace audio
}  // namespace media

#endif  // GARNET_BIN_MEDIA_AUDIO_SERVER_AUDIO_OBJECT_H_
