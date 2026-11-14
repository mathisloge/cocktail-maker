'use strict'

// Makes the group frames of D2 sequence diagrams visible on the dark page.
//
// D2 v0.9.0 draws every group of a sequence diagram (`alt`, `else`, `opt`, `loop`) inside `<g class="shape blend">`
// and always embeds the rule below in its SVG. No D2 setting changes it. On the ink background, `multiply` at half
// opacity pulls the frame and its fill to almost black, so the frames disappear.
// `lighten` shows the amber frame and the fill at their palette colours, because both are lighter than ink, and the
// lifelines under a group still show through it. Deleting the rule instead would hide the lifelines.
//
// asciidoctor-kroki adds each fetched diagram to the content catalog as an image file while the pages are converted,
// so the rewrite runs on `documentsConverted`, when every diagram is in the catalog and nothing is published yet.
// If a D2 SVG has a `blend` group but not this exact rule, a D2 update changed it, and the build fails.

const D2_RULE = '.blend {\n  mix-blend-mode: multiply;\n  opacity: 0.5;\n}'
const VISIBLE_RULE = '.blend {\n  mix-blend-mode: lighten;\n}'

const isD2 = (svg) => svg.includes('data-d2-version=')
const hasBlendGroup = (svg) => /class="[^"]*\bblend\b[^"]*"/.test(svg)

// The SVG with the D2 rule replaced, or null when it has a `blend` group without the exact rule.
// An SVG that already has the visible rule comes back unchanged, so a second run does not fail.
function rewriteBlend (svg) {
  if (svg.includes(D2_RULE)) return svg.replaceAll(D2_RULE, VISIBLE_RULE)
  if (svg.includes(VISIBLE_RULE)) return svg
  return hasBlendGroup(svg) ? null : svg
}

function register () {
  this.once('documentsConverted', ({ contentCatalog }) => {
    const unknown = []
    for (const file of contentCatalog.findBy({ family: 'image' })) {
      const svg = file.contents.toString()
      if (!file.src.basename.endsWith('.svg') || !isD2(svg)) continue
      const rewritten = rewriteBlend(svg)
      if (rewritten === null) unknown.push(`${file.src.component}:${file.src.module}:image$${file.src.relative}`)
      else if (rewritten !== svg) file.contents = Buffer.from(rewritten)
    }
    if (unknown.length) {
      throw new Error(
        `d2-blend: these D2 diagrams have a blend group but not the rule ${JSON.stringify(D2_RULE)}. ` +
          `D2 changed how it draws sequence diagram groups, so update docs/ui/extensions/d2-blend.js:\n  ${unknown.join('\n  ')}`
      )
    }
  })
}

module.exports = { register, rewriteBlend, D2_RULE, VISIBLE_RULE }
