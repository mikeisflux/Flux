---
name: Find the communities my buyers are already in
command: market-find-buyer-communities
description: A vetted list of forums, subreddits, and groups where your audience gathers, with each one's promotion rules
categories: [Marketing, Research]
roles: [marketing, founders]
worksWith:
  - id: Reddit
    transport: browser
  - id: Sheets
    transport: api
writeScope: readonly
body_status: authored
---
## When to use

The user has a product and needs to find where the people who buy things like
it already spend time — before spending anything on ads.

## Find them by proxy, not by keyword

Searching for the product category finds the obvious two communities everyone
already knows. The useful ones are found by proxy:

1. **Comparable products.** Search for people discussing similar products by
   name. Wherever that conversation happens is where the buyers are.
2. **Adjacent creators.** Find who else makes something similar and look at
   where they post and get engagement.
3. **The problem, not the product.** People discuss the need long before they
   know the category exists.
4. **Follow the crossover.** Once a community is found, look at what else its
   members reference. Communities cluster.

Cover the platforms separately — a forum, a subreddit, a Discord, and a
Facebook group serve different slices and rarely overlap as much as expected.

## Vet each one before recommending it

A large dead community is worth less than a small active one. For each:

- **Members and actual activity** — posts per week, not member count. Many
  large communities are archives.
- **Engagement shape** — do posts get replies, or is it a wall of links?
- **Whether the audience buys.** Look for purchase talk: recommendations,
  hauls, "where did you get". A community that discusses without buying is a
  poor target however large.

## Read the promotion rules — this is the important part

Almost every community has rules on self-promotion, and most ban it outright
or restrict it to a weekly thread. **Getting banned burns that community
permanently**, and there is no appeal worth the effort.

For each community, record:

- The actual rule, quoted from the sidebar or pinned post
- Whether there is a designated promo thread or day
- Whether flair or a tag is required
- Any participation requirement (account age, karma, prior posting)

Then classify: **participate only**, **promo thread only**, or **open**.

## Recommend an approach, not a blast

The output is a ranked sheet plus a recommendation per community. For the
`participate only` ones — usually the best ones — the recommendation is to
show up as a person: answer questions in the niche, post work in progress
where that is welcome, and let the product surface naturally. That takes weeks
and it is what actually works.

Never recommend posting the same message across multiple communities. It is
detected instantly by the people who moderate them, and it is the fastest way
to lose all of them at once.

## Heuristics

- One community done well beats ten done shallowly.
- Read a month of top posts before recommending anything. Fit is obvious once
  you have.
- If the rules are ambiguous, treat it as no-promotion.

## Gotchas

Discord and private groups are invisible to search. Find them via the public
communities that link to them, or via creators' link pages.
