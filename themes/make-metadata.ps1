Get-ChildItem -Directory | ForEach-Object {
    $metadata = Get-Content "$_/metadata.json" | ConvertFrom-Json
    $metadata
    $name = $metadata.identifier -replace "-themes?"
    $data = [ordered]@{
        id          = "mo2-theme-" + $name;
        type        = "theme";
        name        = $metadata.name;
        description = $metadata.description;
        version     = "1.0.0";
        content     = @{
            themes = $metadata.themes;
        };
    }
    $data
    # ConvertTo-Json -InputObject $data #  | Set-Content (Join-Path $_ "metadata.json")
}

# $fixNames = @{
#     dark      = "Dark";
#     dracula   = "Dracula";
#     nighteyes = "Night Eyes";
#     parchment = "Parchment";
#     skyrim    = "Skyrim";
# }

# Get-ChildItem -Directory -Exclude "vs15" | ForEach-Object {
#     $name = $fixNames[$_.Name];
#     $data = [ordered]@{
#         identifier  = "mo2-theme-" + $_.Name;
#         type        = "theme";
#         name        = "$name Theme";
#         description = "$name theme for ModOrganizer2.";
#         version     = "1.0.0";
#         content     = @{
#             themes = @{
#                 $_.Name = [ordered]@{
#                     name = $name;
#                     path = (Get-ChildItem $_ -Filter "*.qss")[0].Name;
#                 }
#             };
#         };
#         ;
#     }
#     ConvertTo-Json -InputObject $data | Set-Content (Join-Path $_ "metadata.json")
# }
