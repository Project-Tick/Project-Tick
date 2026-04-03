# Contributing to MNV

Patches are welcome in whatever form.
Discussions about patches happen on the [mnv-dev][0] mailing list.
If you create a pull request on GitHub it will be
forwarded to the mnv-dev mailing list. You can also send your patch there
directly (but please note, the initial posting is subject to moderation).
In that case an attachment with a unified diff format is preferred.
Information about the mailing list can be found [on the MNV website][0]

A pull request has the advantage that it will trigger the Continuous
Integration tests, you will be warned of problems (you can ignore the coverage
warning, it's noisy).

Please always add a test, if possible. All new functionality should be tested
and bug fixes should be tested for regressions: the test should fail before the
fix and pass after the fix. Look through recent patches for examples and find
help with ":help testing". The tests are located under "src/testdir".

Contributions will be distributed with MNV under the MNV license. Providing a
change to be included implies that you agree with this and your contribution
does not cause us trouble with trademarks or patents. There is no CLA to sign.

## Signing-off commits

While not required, it's recommended to use **Signed-off commits** to ensure
transparency, accountability, and compliance with open-source best practices.
Signed-off commits follow the [Developer Certificate of Origin (DCO)][15],
which confirms that contributors have the right to submit their changes under
the project's license. This process adds a `Signed-off-by` line to commit
messages, verifying that the contributor agrees to the project's licensing
terms. To sign off a commit, simply use the -s flag when committing:

```sh
git commit -s
```

This ensures that every contribution is properly documented and traceable,
aligning with industry standards used in projects like the Linux Kernel or
the git project. By making Signed-off commits a standard practice, we help
maintain a legally compliant and well-governed codebase while fostering trust
within our contributor community.

When merging PRs into MNV, the current maintainer @chrisbra usually adds missing
`Signed-off-by` trailers for the author user name and email address as well for
anybody that explicitly *ACK*s a pull request as a statement that those
approvers are happy with that particular change.

## Using AI

When using AI for contributions, please disclose this. Any AI-generated code
must follow the MNV code style. In particular, [test_codestyle.mnv][18]
must not report any failures. Check the CI output for any test failures.

Ensure that changes are properly tested. Do not submit a single PR that
addresses multiple unrelated issues.

# Reporting issues

We use GitHub [issues][17], but that is not a requirement. Writing to the MNV
mailing list is also fine.

Please use the GitHub issues only for actual issues. If you are not 100% sure
that your problem is a MNV issue, please first discuss this on the MNV user
mailing list. Try reproducing the problem without any of your plugins or settings:

    mnv --clean

If you report an issue, please describe exactly how to reproduce it.
For example, don't say "insert some text" but say what you did exactly:
`ahere is some text<Esc>`.
Ideally, the steps you list can be used to write a test to verify the problem
is fixed.

Feel free to report even the smallest problem, also typos in the documentation.

You can find known issues in the todo file: `:help todo`.
Or open [the todo file][todo list] on GitHub to see the latest version.

# Syntax, indent and other runtime files

The latest version of these files can be obtained from the repository.
They are usually not updated with numbered patches. However, they may
or may not work with older MNV releases (since they may depend on new
features).

If you find a problem with one of these files or have a suggestion for
improvement, please first try to contact the maintainer directly.
Look in the header of the file for the name, email address, github handle and/or
upstream repository. You may also check the [MAINTAINERS][11] file.

The maintainer will take care of issues and send updates to the MNV project for
distribution with MNV.

If the maintainer does not respond, contact the [mnv-dev][0] mailing list or
open an [issue][17] here.

Note: Whether or not to use MNV9 script is up to the maintainer. For runtime
files maintained here, we aim to preserve compatibility with Neomnv if
possible. Please wrap MNV9 script with a guard like this:
```mnv
if has('mnv9script')
   " use MNV9 script implementation
   [...]
endif
```

## Contributing new runtime files

If you want to contribute new runtime files for MNV or Neomnv, please create a
PR with your changes against this repository here. For new filetypes, do not forget:

- to add a new [filetype test][12] (keep it similar to the other filetype tests).
- all configuration switches should be documented
  (check [filetype.txt][13] and/or [syntax.txt][14] for filetype and syntax plugins)
- add yourself as Maintainer to the top of file (again, keep the header similar to
  other runtime files)
- add yourself to the [MAINTAINERS][11] file.
- add a guard `if has('mnv9script')` if you like to maintain Neomnv
  compatibility but want to use MNV9 script (or restrict yourself to legacy MNV
  script)

# Translations

Translating messages and runtime files is very much appreciated! These things
can be translated:

- Messages in MNV, see [src/po/README.txt][1]
  Also used for the desktop icons.
- Menus, see [runtime/lang/README.txt][2]
- MNV tutor, see [runtime/tutor/README.txt][3]
- Manual pages, see [runtime/doc/\*.1][4] for examples
- Installer, see [nsis/lang/README.txt][5]

The help files can be translated and made available separately.
See https://www.mnv.org/translations.php for examples.

# How do I contribute to the project?

Please have a look at the following [discussion][6], which should give you some
ideas. Please also check the [develop.txt][7] helpfile for the recommended
coding style. Often it's also beneficial to check the surrounding code for the style
being used.

For the recommended documentation style, please check [helphelp.txt][16].

# I have a question

If you have some question on the style guide, please contact the [mnv-dev][0]
mailing list. For other questions you can join [`#mnv`][19], use the 
[Vi Stack Exchange][8] website, the [mnv-use][9] mailing list or make use of the 
[discussion][10] feature here at github.

[todo list]: https://github.com/Project-Tick/Project-Tick/blob/master/runtime/doc/todo.txt
[0]: http://www.mnv.org/maillist.php#mnv-dev
[1]: https://github.com/Project-Tick/Project-Tick/blob/master/src/po/README.txt
[2]: https://github.com/Project-Tick/Project-Tick/blob/master/runtime/lang/README.txt
[3]: https://github.com/Project-Tick/Project-Tick/blob/master/runtime/tutor/README.txt
[4]: https://github.com/Project-Tick/Project-Tick/blob/master/runtime/doc/mnv.1
[5]: https://github.com/Project-Tick/Project-Tick/blob/master/nsis/lang/README.txt
[6]: https://github.com/Project-Tick/Project-Tick/discussions/13087
[7]: https://github.com/Project-Tick/Project-Tick/blob/master/runtime/doc/develop.txt
[8]: https://vi.stackexchange.com
[9]: http://www.mnv.org/maillist.php#mnv-use
[10]: https://github.com/Project-Tick/Project-Tick/discussions
[11]: https://github.com/Project-Tick/Project-Tick/blob/master/.github/MAINTAINERS
[12]: https://github.com/Project-Tick/Project-Tick/blob/master/src/testdir/test_filetype.mnv
[13]: https://github.com/Project-Tick/Project-Tick/blob/master/runtime/doc/filetype.txt
[14]: https://github.com/Project-Tick/Project-Tick/blob/master/runtime/doc/syntax.txt
[15]: https://en.wikipedia.org/wiki/Developer_Certificate_of_Origin
[16]: https://github.com/Project-Tick/Project-Tick/blob/master/runtime/doc/helphelp.txt
[17]: https://github.com/Project-Tick/Project-Tick/issues
[18]: https://github.com/Project-Tick/Project-Tick/blob/master/src/testdir/test_codestyle.mnv
[19]: https://web.libera.chat/#mnv
