# Cube fonts

`builtin10.ts` ships an original 10×10 pixel font (digits, A–Z, basic punctuation) so the Text Marquee pattern works out of the box.

## Adding Mono10 (or any other 10px font)

The `text-3d` pattern imports its glyphs from `builtin10.ts`. To use a different font:

1. Download the TTF (e.g. https://jdjimenez.itch.io/mono10 — pay-what-you-want, $0 is fine).
2. Convert each printable ASCII glyph to a 10×10 bitmap with a tool of your choice. The simplest path is a small node script using `opentype.js` + `@napi-rs/canvas`:
   ```js
   import opentype from 'opentype.js';
   import { createCanvas } from '@napi-rs/canvas';
   const font = await opentype.load('data/Mono10.ttf');
   const cv = createCanvas(10, 10);
   const ctx = cv.getContext('2d');
   for (const ch of ' 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,!?-:') {
     ctx.fillStyle = 'black';
     ctx.fillRect(0, 0, 10, 10);
     ctx.fillStyle = 'white';
     // Adjust size + baseline until output matches the font's intended pixel layout.
     font.draw(ctx, ch, 0, 9, 10, { kerning: false });
     const px = ctx.getImageData(0, 0, 10, 10).data;
     // Threshold each pixel's alpha/luma to 0/1 and dump as 10 strings.
   }
   ```
3. Save the result as `mono10.ts` next to `builtin10.ts`, exporting the same `getGlyph` / `FONT_W` / `FONT_H` shape.
4. In `src/shared/patterns/text3d.ts`, replace the `builtin10` import with `mono10`.

You don't need to commit the TTF or the generated `mono10.ts` — both are local-only. Mono10 is licensed PWYW from itch.io; check the font's own license before redistributing converted bitmaps.
