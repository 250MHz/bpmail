---
name: Bug report
about: Create a report to help us improve
title: ''
labels: ''
assignees: ''

---

## Environment

```sh
# Run this script in the bpmail Git repository. Replace this with the output
date -u
git show --format="commit: %H" --no-patch
printf "v\n\q\n" | ionadmin | head -1 | cut -c 3-
uname -ms
```

## Description

<!-- Describe the bug and the expected behavior -->

## To reproduce

<!-- Steps to reproduce the bug. Provide screenshots / links / files / etc. if needed -->
