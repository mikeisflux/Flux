---
name: Post to Facebook
command: social-post-facebook
description: Publish a post to your Facebook profile from your logged-in session
categories: [Marketing, Personal]
roles: [marketing, founders]
worksWith:
  - id: facebook
    transport: browser
writeScope: send              # publishes publicly; gated on approval
limits:
  max_per_day: 3
  min_spacing_minutes: 90
  one_post_per_run: true
related: [social-post-linkedin, social-schedule-week, marketing-brief-feeds]
---

## When to use

Publishing a post you have already written to Facebook — either a Page you
manage or your own profile — without opening the site yourself. Also used as
the final step of a scheduled workflow that drafts content earlier in the run.

Not for: bulk posting across many accounts, engagement automation (likes,
follows, comments at scale), or posting as anyone other than the signed-in
user. Those are what account bans are for.

## Transport

**This skill runs through the browser, against the signed-in personal
profile.** Facebook removed `publish_actions` for personal profiles in 2018,
so there is no API for this — driving the logged-in session is the only route.

Limits are in `limits:` above: three posts a day, 90 minutes apart, one per
run.

## Posting through the browser

1. **Confirm the session.** Navigate to `https://www.facebook.com/` and
   `read_page`. If a login form is present, stop and tell the user to sign in —
   never attempt to authenticate on their behalf, and never touch stored
   credentials.

2. **Confirm the identity.** Read the account name from the page and state it
   back: *"Posting as Mike Wheeler."* Posting to the wrong profile or Page is
   the single worst failure mode here and it is silent. If the user asked for a
   Page, switch to it explicitly and re-confirm.

3. **Open the composer.** Find the element whose accessible name matches
   `What's on your mind` (profile) or `Create post` (Page) and click it. Use
   the node id from the snapshot — never a CSS selector; Facebook's class names
   are obfuscated and rotate.

4. **Enter the text.** Type into the composer's `textbox` role. Facebook's
   composer is a `contenteditable`, not an `<input>`, so a value assignment
   does nothing — the text must be typed. Pace it like a human; instant fill of
   a long post is a strong bot signal.

5. **Attach the image.** Click `Photo/video`, then hand the file path to the
   file chooser. **Wait for the thumbnail to finish rendering before doing
   anything else** — submitting mid-upload posts the caption with no image,
   and for visual work that is a wasted post rather than a partial one. Verify
   the thumbnail is present in the snapshot, not merely that the upload
   control was clicked.

   Multiple images go in one post, not several: click `Photo/video` once and
   add each file. Separate posts for a single page-set fragments reach and
   burns the daily cap.

6. **Verify before submitting.** `read_page` and check the composer contains
   the intended text, the audience selector reads as intended, and no unrelated
   link preview attached itself. Facebook auto-attaches a preview to any URL in
   the body, which frequently is not what the user wanted.

7. **Set the audience to Public — check this every time.** The composer
   remembers whatever audience was used last, and it is not necessarily what
   this post needs. A promotional post left on *Friends* reaches a few hundred
   people who already know you, cannot be shared onward by anyone outside that
   list, and will not surface in search or hashtag results. The post looks
   successful and accomplishes nothing. Read the audience control in the
   snapshot, and if it is not `Public`, change it before submitting.

8. **Request approval.** This is a `send`-scope action: the runner blocks here
   and shows the user the exact text, the target identity, and the audience.
   Nothing is published without an explicit approve.

9. **Submit and confirm.** Click `Post`. Wait for the composer to close and the
   post to appear in the feed. **Do not report success until you have seen the
   published post** — a closed dialog is not proof; Facebook silently discards
   posts it flags.

## Heuristics

- **Verify identity out loud, every time.** More damage comes from posting to
  the right service as the wrong identity than from any other error here.
- **A closed composer is not a published post.** Always read back the feed.
- **Never retry a submit blindly.** If the outcome is unclear, read the feed
  first — a blind retry is how you get duplicate posts.
- **Stop on any interstitial.** If a checkpoint, "confirm it's you", or CAPTCHA
  screen appears, abort the run and tell the user. Do not attempt to work
  through it.
- **One post per run.** Batching posts in a single session is the pattern
  detection is tuned for.

## Gotchas

- **The composer is `contenteditable`.** Setting `.value` silently does nothing
  and the post submits empty.
- **Link previews attach themselves.** Any URL in the body pulls a preview. If
  the user wanted a plain text post, remove it before submitting.
- **Audience is sticky.** The composer remembers the last audience used, which
  may not be what this post should be. Read it, don't assume it.
- **Scheduled posts are a different flow** — the Page composer's schedule
  option, not the normal Post button. Do not conflate them.
- **The DOM changes constantly.** This skill is deliberately written against
  accessible names and roles rather than structure, which is why it survives
  redesigns that break selector-based scrapers. When a step fails, re-read the
  page and look for the control by what it *says*, not where it was.
