# dtn7-bundle

C++ implementation of a DTN bundle as defined in RFC9171.

This implementation is used as part of [dtn7-esp](/dtn7-esp/..) and not intended to be used separately.

The project is setup as an idf-component and can be automatically installed via the idf-component manager.
Add and adapt the following to a project's `idf_component.yml`:
```yml
dependencies:
    dtn7-esp:
        git: "https://github.com/fjrefluxx/DTN7-ESP.git"
        path: "dtn7-bundle"
```
