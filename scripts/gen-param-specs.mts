/**
 * Generate firmware/lib/core/cube_param_specs.h from the TS pattern metas —
 * single source of truth for parameter names/types/ranges/defaults on both
 * sides. Run with: pnpm exec tsx scripts/gen-param-specs.mts
 */
import fs from 'node:fs';
import { patterns } from '../src/shared/patterns/index.js';

const esc = (s: string) => s.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
const f = (n: number) => (Number.isInteger(n) ? n.toFixed(1) : String(n)) + 'f';
const TYPE: Record<string, number> = { number: 0, bool: 1, enum: 2, palette: 3, string: 4, color: 5 };

const lines: string[] = [];
lines.push('#pragma once');
lines.push('// GENERATED from src/shared/patterns/* by scripts/gen-param-specs.mts — do not edit.');
lines.push('#include <cstdint>');
lines.push('');
lines.push('namespace cube {');
lines.push('');
lines.push('// type: 0 num, 1 bool, 2 enum, 3 palette, 4 string, 5 color');
lines.push('struct ParamSpec {');
lines.push('  const char* key;');
lines.push('  const char* label;');
lines.push('  uint8_t type;');
lines.push('  float minV, maxV, stepV;');
lines.push('  float defNum;          // number/bool default');
lines.push('  const char* defStr;    // enum/palette/string default');
lines.push('  const char* options;   // comma-separated, enum only');
lines.push('};');
lines.push('');

const entries: { id: string; varName: string; count: number }[] = [];
for (const [id, pat] of Object.entries(patterns)) {
  const varName = 'kSpecs_' + id.replace(/-/g, '_');
  const specs = pat.meta.params;
  lines.push(`static const ParamSpec ${varName}[] = {`);
  for (const p of specs) {
    const t = TYPE[p.type] ?? 0;
    const defNum = typeof p.default === 'number' ? p.default : p.default === true ? 1 : 0;
    const defStr = typeof p.default === 'string' ? p.default : '';
    const options = p.options?.join(',') ?? '';
    lines.push(
      `    {"${esc(p.key)}", "${esc(p.label)}", ${t}, ${f(p.min ?? 0)}, ${f(p.max ?? 0)}, ${f(p.step ?? 0)}, ` +
        `${f(defNum)}, "${esc(defStr)}", "${esc(options)}"},`,
    );
  }
  lines.push('};');
  entries.push({ id, varName, count: specs.length });
}

lines.push('');
lines.push('struct PatternSpecs {');
lines.push('  const char* id;');
lines.push('  const ParamSpec* specs;');
lines.push('  int count;');
lines.push('};');
lines.push('');
lines.push('static const PatternSpecs kPatternSpecs[] = {');
for (const e of entries) {
  lines.push(`    {"${e.id}", ${e.varName}, ${e.count}},`);
}
lines.push('};');
lines.push(`static const int kPatternSpecsCount = ${entries.length};`);
lines.push('');
lines.push('inline const PatternSpecs* specsFor(const char* id) {');
lines.push('  for (int i = 0; i < kPatternSpecsCount; i++) {');
lines.push('    const char* a = kPatternSpecs[i].id;');
lines.push('    const char* b = id;');
lines.push('    while (*a && *a == *b) { a++; b++; }');
lines.push('    if (*a == *b) return &kPatternSpecs[i];');
lines.push('  }');
lines.push('  return nullptr;');
lines.push('}');
lines.push('');
lines.push('}  // namespace cube');
lines.push('');

fs.writeFileSync('firmware/lib/core/cube_param_specs.h', lines.join('\n'));
const total = entries.reduce((a, e) => a + e.count, 0);
console.log(`wrote ${entries.length} patterns, ${total} param specs`);
