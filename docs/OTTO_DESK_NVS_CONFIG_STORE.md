# Otto Desk firmware configuration storage

`main/otto_desk_config_store.*` provides a bounded NVS persistence primitive for the future local configuration service.

## API

- `OttoDeskConfigStore::ValidateSafeJson(json)`: checks the payload limit, complete JSON parsing (including rejection of trailing non-whitespace data), duplicate object keys, exact version-1 object key sets, required field types and fixed board identity, and the current conservative hardware policy.
- `OttoDeskConfigStore::Save(json)`: validates first, then writes the JSON string to NVS namespace `ottdesk`, key `config`, and commits.
- `OttoDeskConfigStore::Load(json)`: reads the stored string and validates it again before returning it. Invalid stored content is cleared from the output string and reported as an error.
- `OttoDeskConfigStore::Clear()`: erases the stored configuration key.

## Safety policy at this stage

The firmware-side validator only accepts the current safe starter state: the expected XiaoZhi v2.5.0/Waveshare profile identity; camera, onboard codec and onboard expander disabled; display, microphone, amplifier, both servos and PIR disabled; all display/audio GPIO map fields present and null; servo and PIR GPIO fields null; and external servo power not marked verified. It rejects missing or unexpected object keys and duplicate keys. This is a conservative acceptance gate, not the complete JSON Schema implementation: it does not yet enable configurable peripherals or replace the host-side schema validator.

The payload limit is 3072 bytes. NVS must already be initialized by the application before calling this class. `ESP_ERR_NVS_NOT_FOUND` from `Load` means no custom configuration has been saved yet.

## Startup integration and remaining work

`main/main.cc` initializes NVS and calls `Load` before `Application::Initialize()`. A valid saved JSON document is currently logged but is **not applied to hardware drivers**. Missing or invalid configuration does not stop the normal application boot.

The local HTTP API, browser JSON editor/import/export, and explicit default-config recovery are not connected yet. Full firmware-side schema validation and a verified board GPIO map are also still pending.
