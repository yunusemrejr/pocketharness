#!/usr/bin/env python3
"""Check the portable workflow documentation shipped with the binary."""
from pathlib import Path
import re

root = Path(__file__).resolve().parent.parent
required = {
    'project-workflows': ['Discover once', 'Implement at the owner', 'Validate the changed behavior', 'Deliver the verified artifact'],
    'bash-workflows': ['bash -n'],
    'local-webapp-workflows': ['loopback'],
    'shared-hosting-deployment': ['Namecheap', 'GoDaddy', 'GitHub'],
    'linux': ['pocket kit sys', 'pressure'],
    'linux-network-engineering': ['pocket kit reach', '--any-status', 'deadline'],
    'search-discoverability': ['pocket kit seo', 'source', 'advisory'],
    'natural-editorial-writing': ['pocket kit quality', 'facts'],
    'threejs': ['pocket kit asset inspect', 'assets/model-preview.html'],
}
for name, snippets in required.items():
    path = root / 'skills' / name / 'SKILL.md'
    text = path.read_text()
    if not text.startswith('---\n') or f'name: {name}\n' not in text:
        raise SystemExit(f'{path}: invalid skill frontmatter')
    for snippet in snippets:
        if snippet not in text:
            raise SystemExit(f'{path}: missing workflow contract {snippet}')
    for markdown in path.parent.rglob('*.md'):
        for target in re.findall(r'\]\(([^)]+)\)', markdown.read_text()):
            if ':' in target or target.startswith('#'):
                continue
            if not (markdown.parent / target.split('#', 1)[0]).exists():
                raise SystemExit(f'{markdown}: missing reference {target}')
recipes = (root / 'skills/project-workflows/references/stacks.md').read_text()
for stack in ['PHP 8+', 'Node.js', 'Vanilla frontend', 'React through CDN', 'React with Node', 'Go', 'Rust', 'Java', 'Python/Flask', 'Bash', 'C/C++', 'Linux-native', 'Local webapp', 'Algorithm design', 'AI/ML design', 'Local/Colab']:
    if f'| {stack}' not in recipes:
        raise SystemExit(f'missing stack recipe: {stack}')
print('workflow contracts and linked recipes passed')
