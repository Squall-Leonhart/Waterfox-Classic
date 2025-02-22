/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbortSignal.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/AbortSignalBinding.h"

namespace mozilla {
namespace dom {

// AbortSignalImpl
// ----------------------------------------------------------------------------

AbortSignalImpl::AbortSignalImpl(bool aAborted)
  : mAborted(aAborted)
{}

bool
AbortSignalImpl::Aborted() const
{
  return mAborted;
}

void
AbortSignalImpl::Abort()
{
  if (mAborted) {
    return;
  }

  mAborted = true;

  // We might be deleted as a result of aborting a follower, so ensure we live
  // until all followers have been aborted.
  RefPtr<AbortSignalImpl> pinThis = this;

  nsTObserverArray<AbortFollower*>::ForwardIterator iter(mFollowers);
  while (iter.HasMore()) {
    iter.GetNext()->Abort();
  }
}

void
AbortSignalImpl::AddFollower(AbortFollower* aFollower)
{
  MOZ_DIAGNOSTIC_ASSERT(aFollower);
  if (!mFollowers.Contains(aFollower)) {
    mFollowers.AppendElement(aFollower);
  }
}

void
AbortSignalImpl::RemoveFollower(AbortFollower* aFollower)
{
  MOZ_DIAGNOSTIC_ASSERT(aFollower);
  mFollowers.RemoveElement(aFollower);
}

// AbortSignal
// ----------------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(AbortSignal)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(AbortSignal,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFollowingSignal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(AbortSignal,
                                                DOMEventTargetHelper)
  tmp->Unfollow();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(AbortSignal)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(AbortSignal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(AbortSignal, DOMEventTargetHelper)

AbortSignal::AbortSignal(nsIGlobalObject* aGlobalObject,
                         bool aAborted)
  : DOMEventTargetHelper(aGlobalObject)
  , AbortSignalImpl(aAborted)
{}

JSObject*
AbortSignal::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AbortSignalBinding::Wrap(aCx, this, aGivenProto);
}

void
AbortSignal::Abort()
{
  AbortSignalImpl::Abort();

  EventInit init;
  init.mBubbles = false;
  init.mCancelable = false;

  RefPtr<Event> event =
    Event::Constructor(this, NS_LITERAL_STRING("abort"), init);
  event->SetTrusted(true);

  bool dummy;
  DispatchEvent(event, &dummy);
}

// AbortFollower
// ----------------------------------------------------------------------------

AbortFollower::~AbortFollower()
{
  Unfollow();
}

void
AbortFollower::Follow(AbortSignalImpl* aSignal)
{
  MOZ_DIAGNOSTIC_ASSERT(aSignal);

  Unfollow();

  mFollowingSignal = aSignal;
  aSignal->AddFollower(this);
}

void
AbortFollower::Unfollow()
{
  if (mFollowingSignal) {
    mFollowingSignal->RemoveFollower(this);
    mFollowingSignal = nullptr;
  }
}

bool
AbortFollower::IsFollowing() const
{
  return !!mFollowingSignal;
}

} // dom namespace
} // mozilla namespace
