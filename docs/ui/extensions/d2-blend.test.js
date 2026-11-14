'use strict'

// Run with `node --test 'docs/ui/extensions/*.test.js'`. The pytest suite runs it too when Node is installed.

const test = require('node:test')
const assert = require('node:assert/strict')
const { register, rewriteBlend, D2_RULE, VISIBLE_RULE } = require('./d2-blend.js')

const d2Svg = (style, body = '') =>
  `<svg data-d2-version="v0.9.0"><style type="text/css"><![CDATA[\n${style}\n]]></style>${body}</svg>`
const GROUP = '<g class="shape blend"><rect stroke="#c8913a" /></g>'

// Runs the extension on a fake content catalog with the given image files and returns their contents afterwards.
function runExtension (files) {
  let listener
  register.call({ once: (event, fn) => event === 'documentsConverted' && (listener = fn) })
  const images = files.map(([basename, text]) => ({
    src: { component: 'station', module: 'ROOT', basename, relative: basename },
    contents: Buffer.from(text),
  }))
  listener({ contentCatalog: { findBy: ({ family }) => (family === 'image' ? images : []) } })
  return images.map((file) => file.contents.toString())
}

test('replaces the D2 rule with lighten', () => {
  assert.equal(rewriteBlend(d2Svg(D2_RULE, GROUP)), d2Svg(VISIBLE_RULE, GROUP))
})

test('a second rewrite keeps the SVG unchanged', () => {
  const once = rewriteBlend(d2Svg(D2_RULE, GROUP))
  assert.equal(rewriteBlend(once), once)
})

test('keeps an SVG without a blend group and without the rule', () => {
  const svg = d2Svg('.shape { shape-rendering: geometricPrecision; }')
  assert.equal(rewriteBlend(svg), svg)
})

test('rejects a blend group whose rule is not the exact D2 rule', () => {
  assert.equal(rewriteBlend(d2Svg('.blend { mix-blend-mode: multiply; opacity: 0.5; }', GROUP)), null)
  assert.equal(rewriteBlend(d2Svg('', GROUP)), null)
})

test('the extension rewrites only D2 SVG files', () => {
  const other = `<svg><style>${D2_RULE}</style></svg>`
  assert.deepEqual(
    runExtension([
      ['seq-1.svg', d2Svg(D2_RULE, GROUP)],
      ['logo.svg', other],
      ['photo.png', d2Svg(D2_RULE, GROUP)],
    ]),
    [d2Svg(VISIBLE_RULE, GROUP), other, d2Svg(D2_RULE, GROUP)]
  )
})

test('the extension fails the build on an unknown blend rule', () => {
  assert.throws(() => runExtension([['seq-1.svg', d2Svg('', GROUP)]]), /d2-blend: .*\n {2}station:ROOT:image\$seq-1\.svg/)
})
