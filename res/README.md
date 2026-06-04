# res/

Plugin runtime resources loaded by Rack: the module panel SVGs (`*.svg`) and
`fonts/`. The SVGs are generated from `panelkit/specs/` via the panelkit toolkit
— edit the specs, not the SVGs directly. (`res/` stays at the plugin root because
Rack loads panels from here; it is the shipped resource dir.)
