# Otto Desk firmware configuration storage

`main/otto_desk_config_store.*` provides a small NVS persistence primitive for the future local configuration service.

## API

- `OttoDeskConfigStore::ValidateSafeJson(json)`: checks size, complete JSON parsing (including rejection of trailing non-whitespace data), schema version 1, and the current conservative hardware policy.
- `OttoDeskConfigStore::Save(json)`: validates first, then writes the JSON string to NVS namespace `ottdesk`, key `config`, and commits.
- `OttoDeskConfigStore::Load(json)`: reads the stored string and validates it again before returning it. Invalid stored content is cleared from the output string and reported as an error.
- `OttoDeskConfigStore::Clear()`: erases the stored configuration key.

## Safety policy at this stage

The firmware-side validator only accepts the current safe starter state: camera/onboard codec/onboard expander disabled; display, microphone, amplifier, both servos and PIR disabled; every declared GPIO value null; external servo power not marked verified. This intentionally prevents an imported file from energizing hardware before the exact header map and electrical checks are approved.

The payload limit is 3072 bytes. NVS must already be initialized by the application before calling this class. `ESP_ERR_NVS_NOT_FOUND` from `Load` means no custom configuration has been saved yet; the caller must decide how to load the compiled-in default.

## Not yet connected

This class is a storage/validation primitive, not the complete configuration feature. It is not yet called during board startup, and no HTTP routes or browser JSON editor are connected to it. It also does not replace the host-side JSON Schema validator.