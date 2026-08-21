---
name: Post to Facebook
command: social-post-facebook
description: Publish a post to your Facebook profile or Page from your logged-in session
categories: [Marketing, Personal]
roles: [marketing, founders]
worksWith:
  - id: facebook
    transport: browser        # personal profiles: no sanctioned API exists
  - id: facebook_pages
    transport: api            # Pages: Graph API /{page-id}/feed
writeScope: send              # publishes publicly; gated on approval
risk:
  level: high
  reason: >
    Facebook's Terms of Service prohibit automated interaction with the site.
    Browser-driven posting to a personal profile can trigger a checkpoint,
    temporary restriction, or permanent account loss. Posting to a Page via the
    Graph API is sanctioned and carries none of this risk.
  mitigations:
    - Prefer the Graph API path whenever the target is a Page.
    - Hard cap of 3 posts per day, minimum 90 minutes apart.
    - Human-paced typing and dwell; never instant form fill.
    - Abort immediately on any checkpoint or unusual-activity screen.
related: [social-post-linkedin, social-schedule-week, marketing-brief-feeds]
---

## When to use

Publishing a post you have already written to Facebook — either a Page you
manage or your own profile — without opening the site yourself. Also used as
the final step of a scheduled workflow that drafts content earlier in the run.

Not for: bulk posting across many accounts, engagement automation (likes,
follows, comments at scale), or posting as anyone other than the signed-in
user. Those are what account bans are for.

## Choose the transport first

This decides everything downstream, so resolve it before doing anything else.
**Default to the API path.** Only fall through to the browser when the target
is a personal profile, which genuinely has no API.

**If the target is a Page → use the Graph API.** It is supported, documented,
rate-limited rather than ban-triggering, and does not break when Facebook
redesigns. Requires a Page access token with `pages_manage_posts`.

```
POST https://graph.facebook.com/v21.0/{page-id}/feed
  message=<text>
  link=<optional url>
  access_token=<page token>
```

**Anything schedulable belongs on the API path.** The Graph API schedules
natively (10 minutes to 6 months out), so a recurring "post weekly" workflow
never needs a browser open, never needs this machine awake at post time, and
never accrues detection risk. If the user asks to schedule profile posts,
propose moving that content to a Page rather than running a browser job.

**If the target is a personal profile → browser only.** Facebook removed
`publish_actions` for personal profiles in 2018; there is no API. Driving the
logged-in session is the only route, and it is the risky one. Say so to the
user before the first run, not after.

## Quickstart: a small business posting a few times a day

This is the common case — one person promoting their own work, a handful of
posts a day, images doing most of the selling. **It runs entirely on the API
path.** No browser automation, no ToS exposure, no detection question, and the
machine does not need to be awake when the posts go out.

Setup is one-time, maybe twenty minutes:

1. **Post as a Page, not a personal profile.** For a business this is correct
   on every axis, independent of automation: Insights and reach data, native
   scheduling, no 5,000-friend ceiling, no risk of a personal account
   restriction taking the business offline with it. Personal profiles have no
   API at all, so posting to one is the *only* reason to touch the browser.

2. **Create a Meta app** at developers.facebook.com, add the *Facebook Login*
   product, and request `pages_show_list`, `pages_manage_posts`, and
   `pages_read_engagement`. As the Page admin you do not need App Review for
   your own Page.

3. **Get a non-expiring Page token.** Exchange the short-lived user token for a
   long-lived one, then derive the Page token from it — Page tokens obtained
   this way do not expire, so this is genuinely a one-time step:
   ```
   GET /oauth/access_token?grant_type=fb_exchange_token&fb_exchange_token=<short>
   GET /me/accounts        # returns the Page token
   ```

4. **Queue the day's posts.** Schedule between 10 minutes and 6 months ahead:
   ```
   POST /{page-id}/photos
     url=<image url>            (or source=<upload>)
     caption=<text>
     published=false
     scheduled_publish_time=<unix seconds>
   ```
   Use `/photos` rather than `/feed` for image posts — for visual work the
   image is the post, and `/feed` with a `link` produces a link preview card
   instead of a full-bleed image.

5. **Let the workflow batch it.** A scheduled Flux workflow that runs once each
   morning, picks the day's three images and captions, and queues them all via
   the API is strictly better than three separate runs: one execution, three
   posts, delivered by Facebook whether or not your machine is on.

### What this costs

Three scheduled API posts a day is one short agent run each morning — a few
thousand tokens to select and caption, then three HTTP calls. Well under a
cent a day at current model pricing. The browser path would cost more in
tokens (page snapshots on every step) *and* carry the account risk, for a
worse result.

### When you would still need the browser

- Posting to a personal profile rather than a Page.
- Reading or replying to comments beyond what the API exposes.
- Anything in Groups — the Groups API was substantially closed in 2024.

For a business promoting its own work, none of those are on the critical path.

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

5. **Attach media if asked.** Click `Photo/video`, then hand the file path to
   the file chooser. Wait for the thumbnail to render before continuing —
   submitting during upload posts without the image.

6. **Verify before submitting.** `read_page` and check the composer contains
   the intended text, the audience selector reads as intended, and no unrelated
   link preview attached itself. Facebook auto-attaches a preview to any URL in
   the body, which frequently is not what the user wanted.

7. **Request approval.** This is a `send`-scope action: the runner blocks here
   and shows the user the exact text, the target identity, and the audience.
   Nothing is published without an explicit approve.

8. **Submit and confirm.** Click `Post`. Wait for the composer to close and the
   post to appear in the feed. **Do not report success until you have seen the
   published post** — a closed dialog is not proof; Facebook silently discards
   posts it flags.

## Heuristics

- **Verify identity out loud, every time.** More damage comes from posting to
  the right service as the wrong identity than from any other error here.
- **A closed composer is not a published post.** Always read back the feed.
- **Never retry a submit blindly.** If the outcome is unclear, read the feed
  first — a blind retry is how you get duplicate posts.
- **Stop on any interstitial.** Checkpoints, "confirm it's you", CAPTCHAs, and
  unusual-activity screens mean the automation was detected. Abort the run and
  tell the user. Solving them is both futile and a ToS escalation.
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
