$fixNames = @{
    dark      = "Dark";
    dracula   = "Dracula";
    nighteyes = "Night Eyes";
    parchment = "Parchment";
    skyrim    = "Skyrim";
}

Get-ChildItem -Directory -Exclude "vs15" | ForEach-Object {
    $name = $fixNames[$_.Name];
    $data = [ordered]@{
        identifier  = $_.Name + "-theme";
        type        = "theme";
        name        = "$name Theme";
        description = "$name theme for ModOrganizer2.";
        version     = "1.0.0";
        themes      = @{
            $_.Name = [ordered]@{
                name = $name;
                path = (Get-ChildItem $_ -Filter "*.qss")[0].Name;
            }
        };
    }
    ConvertTo-Json -InputObject $data | Set-Content (Join-Path $_ "metadata.json")
}
