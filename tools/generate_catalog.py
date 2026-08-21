#!/usr/bin/env python3
"""Generate runtime catalog data from the reverse-engineered docs.

Metadata (title, outcome, connectors, schedule, category) is TRANSCRIBED from
the reference screenshots. Derived fields (cron, writeScope, transport,
command) are computed here by the rules below and are marked as derived.
"""
import io, json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The 37 connectors that actually exist (docs/05). Everything else in the
# catalog is browser-driven - see the cross-reference in that doc.
API_CONNECTORS = {
    "Google","Docs","Sheets","Gmail","Google Drive","Google Calendar","Slack",
    "iMessage","Outlook","Apollo","Granola","QuickBooks","Notion","Linear",
    "Attio","Stripe","Atlassian","Asana","Superhuman Mail","Sentry",
    "Cloudflare","Zapier","Webflow","GitHub","HubSpot","Monday","Monday.com",
    "Calendly","Airtable","Grain","Salesforce","Brex","Mercury","Ramp",
    "Cal.com","Klaviyo","Coda","PostHog","Pylon","Sanity","DualEntry",
    "Google Analytics","Instagram",
}

DAYS = {"monday":1,"tuesday":2,"wednesday":3,"thursday":4,"friday":5,
        "saturday":6,"sunday":0}

def to_cron(text):
    """Natural-language schedule -> cron. Returns None if unparseable."""
    if not text or text == "—":
        return None
    t = text.lower().strip()

    m = re.search(r'every (\d+) hours?', t)
    if m:
        return "0 */%s * * *" % m.group(1)

    times = []
    for hm, mn, ampm in re.findall(r'(\d{1,2})(?::(\d{2}))?\s*(am|pm)', t):
        h = int(hm) % 12
        if ampm == "pm":
            h += 12
        times.append((h, int(mn or 0)))
    if "noon" in t and not times:
        times = [(12, 0)]
    if not times:
        return None

    minute = times[0][1]
    hours = ",".join(str(h) for h, _ in times)

    dow = "*"
    dom = "*"
    if "weekday" in t:
        dow = "1-5"
    elif "monthly" in t or "of each month" in t or "1st" in t:
        dom = "1"
    else:
        found = [str(v) for k, v in DAYS.items() if k in t]
        if found:
            dow = ",".join(sorted(set(found), key=int))
    return "%d %s %s * %s" % (minute, hours, dom, dow)

SEND = re.compile(r'\b(sent|sends|submitted|applied to|posted|invites?|'
                  r'booked|purchase|pay|payment made)\b')
DRAFT = re.compile(r'\b(draft|drafts|drafted|queued|ready to review|unsent|'
                   r'staged|ready to|prep)\b')

def write_scope(outcome):
    """Derived. The reference states write-safety only in prose; this turns
    that prose into the enum the runner enforces."""
    o = outcome.lower()
    if DRAFT.search(o):
        return "draft"
    if SEND.search(o):
        return "send"
    return "readonly"

def slug(title, category):
    s = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")
    s = re.sub(r'^(build|find|get|make|turn|run|check|draft|write|track|'
               r'source|pull|send|watch|scrape|clean|merge|review|plan|'
               r'search|apply|score|map|vet|spot|brief|mine|catch|flag|'
               r'audit|sweep|triage|refresh|nudge|prep|start|enrich|'
               r'reproduce|resurface|coach|tune|rank|sync|diff|digest|'
               r'extract|export|reconcile|analyze|deep|fact|click)-', "", s)
    words = [w for w in s.split("-") if w not in
             ("my","me","a","an","the","for","into","from","with","and",
              "to","of","in","on","every","that","this","i")]
    return "%s-%s" % (category.lower(), "-".join(words[:4]))

def parse_rows(path, cols):
    """Yield (category, dict) for every table row.

    Handles both table shapes in the docs: numbered (`| 1 | Title | ... |`)
    and unnumbered (`| Title | ... |`), since the sections were transcribed
    at different times.
    """
    category = None
    for line in io.open(path, encoding="utf-8"):
        m = re.match(r'^## ([A-Za-z ]+?) — ', line)
        if m:
            category = m.group(1).strip()
            continue
        if not line.startswith("| ") or line.startswith("|---"):
            continue
        parts = [p.strip() for p in line.strip().strip("|").split("|")]
        if not parts:
            continue
        # Drop the leading index column when present.
        if re.fullmatch(r'\d+', parts[0]):
            parts = parts[1:]
        # Skip header rows.
        if parts[0] in ("Title", "#", "Name"):
            continue
        if len(parts) < len(cols):
            continue
        yield category, dict(zip(cols, parts[:len(cols)]))

def connectors(raw):
    out = []
    for c in (raw or "").split(","):
        c = c.strip()
        if not c or c == "—" or c.startswith("+"):
            continue
        out.append({"id": c, "transport": "api" if c in API_CONNECTORS else "browser"})
    return out

# ---------------------------------------------------------------- templates
templates = []
seen = set()
for cat, row in parse_rows(os.path.join(ROOT, "docs/02-template-catalog.md"),
                           ["title", "outcome", "connectors", "schedule"]):
    if cat in (None, "Featured"):
        continue
    key = (cat, row["title"])
    if key in seen:
        continue
    seen.add(key)
    cron = to_cron(row["schedule"])
    templates.append({
        "id": slug(row["title"], cat),
        "title": row["title"],
        "outcome": row["outcome"],
        "category": cat,
        "connectors": connectors(row["connectors"]),
        "schedule": None if not cron else {
            "cron": cron,
            "display": row["schedule"],
        },
        "writeScope": write_scope(row["outcome"]),
    })

io.open(os.path.join(ROOT, "data/templates.json"), "w", encoding="utf-8").write(
    json.dumps({"version": 1,
                "provenance": {
                    "transcribed": ["title", "outcome", "category",
                                    "connectors.id", "schedule.display"],
                    "derived": ["id", "schedule.cron", "writeScope",
                                "connectors.transport"],
                },
                "templates": templates}, indent=2) + "\n")

by_cat = {}
for t in templates:
    by_cat[t["category"]] = by_cat.get(t["category"], 0) + 1
scopes = {}
for t in templates:
    scopes[t["writeScope"]] = scopes.get(t["writeScope"], 0) + 1

print("templates: %d" % len(templates))
print("  by category:", dict(sorted(by_cat.items())))
print("  by writeScope:", dict(sorted(scopes.items())))
print("  scheduled: %d" % sum(1 for t in templates if t["schedule"]))
print("  browser-only: %d" % sum(
    1 for t in templates
    if t["connectors"] and all(c["transport"] == "browser" for c in t["connectors"])))

# ------------------------------------------------------------------- skills
# Frontmatter is transcribed. Bodies are NOT: only three were ever captured
# (docs/06). Everything else gets a well-formed skeleton whose "When to use"
# is derived from the transcribed description - which doc 06 confirms IS the
# retrieval trigger - plus category-appropriate scaffolding. Each file records
# body_status so authored content is never mistaken for reverse-engineered.

SKILL_ROLES = {
    "Sales": ["sales", "founders"],
    "Recruiting": ["recruiting"],
    "Marketing": ["marketing", "founders"],
    "Data": ["analysts", "engineering"],
    "Research": ["analysts", "founders"],
    "Ops": ["ops", "founders"],
    "Engineering": ["engineering"],
    "Docs": ["everyone"],
    "Personal": ["everyone"],
    "Monitoring": ["ops", "founders"],
}

SCAFFOLD = {
    "Sales": ["Gather context", "Qualify", "Draft the outreach", "Log it"],
    "Recruiting": ["Define the bar", "Source", "Assess against the bar", "Record the decision"],
    "Marketing": ["Understand the audience", "Gather what performed", "Produce", "Measure"],
    "Data": ["Get the data", "Validate it", "Transform", "Deliver"],
    "Research": ["Scope the question", "Gather primary sources", "Cross-check", "Report with citations"],
    "Ops": ["Locate the records", "Reconcile", "Act", "Leave an audit trail"],
    "Engineering": ["Reproduce", "Isolate", "Verify the fix", "Report"],
    "Docs": ["Understand the intent", "Draft", "Revise against the brief", "Deliver"],
    "Personal": ["Clarify the constraints", "Search", "Compare honestly", "Recommend"],
    "Monitoring": ["Collect", "Filter to what changed", "Rank by what matters", "Brief"],
}

def skill_body(name, desc, cat):
    steps = SCAFFOLD.get(cat, SCAFFOLD["Docs"])
    lines = ["## When to use", "", desc.rstrip(".") + ".", ""]
    lines += ["## Approach", ""]
    for i, s in enumerate(steps, 1):
        lines.append("%d. **%s.**" % (i, s))
    lines += ["", "## Heuristics", "",
              "- State what you could not determine rather than filling the gap.",
              "- Cite the source for every claim a reader would want to check.",
              "- Stop and ask when the request is ambiguous in a way that changes the output.",
              "", "## Gotchas", "",
              "Verify the result against its source before reporting it as done."]
    return "\n".join(lines)

skills = []
for cat, row in parse_rows(os.path.join(ROOT, "docs/03-skills-catalog.md"),
                           ["name", "description", "connectors"]):
    if cat is None:
        continue
    conns = connectors(row["connectors"])
    skills.append({
        "name": row["name"],
        "command": slug(row["name"], cat),
        "description": row["description"],
        "category": cat,
        "roles": SKILL_ROLES.get(cat, ["everyone"]),
        "connectors": conns,
        "writeScope": write_scope(row["description"]),
    })

os.makedirs(os.path.join(ROOT, "data/skills"), exist_ok=True)
written = 0
for s in skills:
    path = os.path.join(ROOT, "data/skills", s["command"] + ".md")
    if os.path.exists(path):
        continue  # never clobber a hand-authored skill
    fm = ["---",
          "name: %s" % s["name"],
          "command: %s" % s["command"],
          "description: %s" % s["description"],
          "categories: [%s]" % s["category"],
          "roles: [%s]" % ", ".join(s["roles"])]
    if s["connectors"]:
        fm.append("worksWith:")
        for c in s["connectors"]:
            fm.append("  - id: %s" % c["id"])
            fm.append("    transport: %s" % c["transport"])
    fm += ["writeScope: %s" % s["writeScope"],
           "body_status: skeleton   # frontmatter transcribed; body authored",
           "---", ""]
    io.open(path, "w", encoding="utf-8").write(
        "\n".join(fm) + skill_body(s["name"], s["description"], s["category"]) + "\n")
    written += 1

# Index every skill on disk, not just the generated ones - hand-authored
# skills must load too.
def read_frontmatter(path):
    txt = io.open(path, encoding="utf-8").read()
    if not txt.startswith("---"):
        return {}
    fm = txt.split("---", 2)[1]
    out = {}
    for line in fm.splitlines():
        m = re.match(r'^(\w+):\s*(.+?)\s*(?:#.*)?$', line)
        if m:
            out[m.group(1)] = m.group(2).strip()
    return out

index = []
for fn in sorted(os.listdir(os.path.join(ROOT, "data/skills"))):
    if not fn.endswith(".md"):
        continue
    fmd = read_frontmatter(os.path.join(ROOT, "data/skills", fn))
    if not fmd.get("command"):
        continue
    index.append({
        "file": fn,
        "name": fmd.get("name", ""),
        "command": fmd["command"],
        "category": fmd.get("categories", "").strip("[]"),
        "writeScope": fmd.get("writeScope", "readonly"),
        "bodyStatus": fmd.get("body_status", "authored"),
        "description": fmd.get("description", ""),
    })
io.open(os.path.join(ROOT, "data/skills/index.json"), "w", encoding="utf-8").write(
    json.dumps({"version": 1, "skills": index}, indent=2) + "\n")

cats = {}
for s in skills:
    cats[s["category"]] = cats.get(s["category"], 0) + 1
print("skills: %d parsed, %d files written" % (len(skills), written))
print("  by category:", dict(sorted(cats.items())))
