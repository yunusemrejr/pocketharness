# Headless jobs

Inspect `soffice --version` and local help before relying on a filter. LibreOffice needs a writable profile. Independent automation should use a unique profile URL to avoid attaching to the user's existing instance. Construct that URL with a URI-aware library for paths containing spaces, rather than concatenating an unescaped path. [Official startup parameters](https://help.libreoffice.org/latest/en-US/text/shared/guide/start_parameters.html?DbPAR=SHARED).

Example shape after creating private directories:

```text
soffice -env:UserInstallation=file:///tmp/office-job-UNIQUE/profile --headless --convert-to pdf --outdir /tmp/office-job-UNIQUE/output /absolute/source.docx
```

Replace UNIQUE with a securely created job directory; pass arguments as an argv array from scripts. Use an installed filter appropriate to the input/output types. Output basenames may collide when multiple inputs share a stem: isolate output directories or rename deliberately. Never convert onto the only source copy.

Apply a bounded timeout and terminate only the process group created by the job if it hangs. Record stderr and exit code in a compact diagnostic. Check output freshness, nonzero size, expected extension and that it actually opens; an old file must not make a failed conversion look successful. Clean the private profile only after its process exits.

For PDF exports, render and inspect affected pages, not merely the file header. Do a small representative batch before processing hundreds of inputs. When reproducing a conversion fault, keep the failing input and command metadata private; logs should not dump document content. Do not weaken profile locks or kill an unrelated soffice process as a first recovery step.
