'use strict'

// The sections of the header and footer: the parent `system` and the subprojects.
// The known subprojects come in the reading order of the project, each only if its component exists.
// Every other component joins the subprojects after them, sorted by title.
const PARENT = 'system'
const KNOWN_SUBPROJECTS = ['station', 'embedded', 'mechanics']

const toSection = ({ name, title, url }) => ({ name, title, url })

const byTitle = (a, b) => (a.title < b.title ? -1 : a.title > b.title ? 1 : 0)

module.exports = ({ data: { root } }) => {
  const components = (root.site && root.site.components) || {}
  const system = components[PARENT] ? toSection(components[PARENT]) : null
  const known = KNOWN_SUBPROJECTS.filter((name) => components[name]).map((name) => toSection(components[name]))
  const others = Object.values(components)
    .filter(({ name }) => name !== PARENT && !KNOWN_SUBPROJECTS.includes(name))
    .map(toSection)
    .sort(byTitle)
  return { system, subprojects: [...known, ...others] }
}
