# Release Checklist

We're starting the checklist with another release, another concern.

## Pre-release
[ ] Was the build from the previous commit successful?
[ ] Is the working tree clean?
[ ] Was the version bumped?
[ ] Is the version consistent everywhere?
[ ] How many products have been changed since the last tag? - ( Write here:  )
[ ] Add new product versions to bugzilla

## Content
[ ] Has the ChangeLog been written?
[ ] Have the release notes been written?
[ ] Are the logos up-to-date?
[ ] Are screenshots / metainfo / desktop metadata up-to-date?
[ ] Were licenses / copyright notices reviewed?

## Build & Validation
[ ] Did all required CI jobs pass?
[ ] Were all target artifacts built successfully?
[ ] Were package manifests updated?
[ ] Did validation tools pass? (AppStream, desktop-file-validate, lint, etc.)
[ ] Was a quick smoke test done on the built artifacts?

## Release Assets
[ ] Were checksums generated?
[ ] Were signatures generated and verified?
[ ] Were all files uploaded successfully?
[ ] Was the update feed / latest.json updated?
[ ] Were download links checked after upload?

## Publishing
[ ] Was the tag created correctly?
[ ] Was the release published with the correct title and notes?
[ ] Were downstream packages / stores updated if needed?
[ ] Was rollback / hotfix readiness considered?
