# Structure and revisions

A visible sentence may span multiple runs because of emphasis, hyperlinks, fields, comments or revisions. Setting paragraph.text discards run structure; replacing whole document XML with string substitution can corrupt relationships. Map the exact target range, preserve neighboring runs and edit only the intended content. Tables can nest and text boxes live outside ordinary paragraph lists, so a simple paragraph loop is not a complete document inventory.

Use styles already present in the template. Built-in Word styles are looked up by their English names in python-docx; user-defined styles use their defined names. A style referenced without an actual definition may not behave as expected. [Using styles](https://python-docx.readthedocs.io/en/stable/user/styles-using.html), [style definitions](https://python-docx.readthedocs.io/en/latest/user/styles-understanding.html).

For tracked changes, first establish whether to preserve, add, accept or reject revisions from the user's request. Ordinary python-docx edits are not automatically Word revisions. Use a capable native/connector workflow, or carefully validated OOXML revision elements with author, date and unique identifiers when necessary. Do not flatten preexisting revisions or silently accept them to simplify editing. Check both the final-view text and the revision record.

Fields, content controls, bookmarks and comments have relationships and boundaries. Preserve these when inserting text; verify cross-references after field update in a compatible application. For automated replacements test a document containing a split-run match, a hyperlink, table text and a field adjacent to the target. Keep package relationships intact, then reopen and render before claiming a faithful edit.
