## HeatPumpType:

Assuming that bytes from #129 to #138 are unique for each model of Aquarea heat pump:

| Value |  Bytes 129 - 138 | IDU | ODU | KIT | Power [kW] | 1ph/3ph | T-CAP/HP |
| ---| --- | --- | --- | --- | --- | --- | ---|
| 0 | E2 CF 0B 13 33 32 D1 0C 16 33 | Monoblock | WH-MDC05H3E5 | Monoblock | 5 | 1ph | HP |
| 1 | E2 CF 0B 14 33 42 D1 0B 17 33 | Monoblock | WH-MDC07H3E5 | Monoblock | 7 | 1ph | HP |
| 2 | E2 CF 0D 77 09 12 D0 0B 05 11 | WH-SXC09H3E5 | WH-UX09HE5 | KIT-WXC09H3E5 | 9 | 1ph | T-CAP |
| 3 | E2 CF 0C 88 05 12 D0 0B 97 05 | WH-SDC09H3E8 | WH-UD09HE8 | KIT-WC09H3E8 | 9 | 3ph | HP |
| 4 | E2 CF 0D 85 05 12 D0 0C 94 05 | WH-SXC09H3E8 | WH-UX09HE8 | KIT-WXC09H9E8 | 9 | 3ph | T-CAP |
| 5 | E2 CF 0D 86 05 12 D0 0C 95 05 | WH-SXC12H9E8 | WH-UX12HE8 | KIT-WXC12H9E8 | 12 | 3ph | T-CAP |
| 6 | E2 CF 0D 87 05 12 D0 0C 96 05 | WH-SXC16H9E8 | WH-UX16HE8 | KIT-WXC16H9E8 | 16 | 3ph | T-CAP |
| 7 | E2 CE 0D 71 81 72 CE 0C 92 81 | WH-SDC05H3E5 | WH-UD05HE5 | KIT-WC05H3E5  | 5 | 1ph | HP |
| 8 | 62 D2 0B 43 54 42 D2 0B 72 66 | WH-SDC0709J3E5 | WH-UD09JE5 | KIT-WC09J3E5 | 9 | 1ph | HP |
| 9 | C2 D3 0B 33 65 B2 D3 0B 94 65 | Monoblock | WH-MDC05J3E5 | Monoblock | 5 | 1ph | HP |
|10 | E2 CF 0B 15 33 42 D1 0B 18 33 | Monoblock | WH-MDC09H3E5 | Monoblock | 9 | 1ph | HP |
|11 | E2 CF 0B 41 34 82 D1 0B 31 35 | Monoblock | WH-MXC09H3E5 | Monoblock | 9 | 1ph | T-CAP |
|12 | 62 D2 0B 45 54 42 D2 0B 47 55 | WH-ADC0309J3E5 | WH-UD09JE5 | KIT-ADC09JE5 | 9 | 1ph | HP - All-In-One |
|13 | E2 CF 0C 74 09 12 D0 0D 95 05 | WH-ADC0916H9E8 | WH-UX12HE8 | KIT-AXC12HE8 | 12 | 3ph | T-CAP - All-In-One |
|14 | E2 CF 0B 82 05 12 D0 0C 91 05 | WH-SQC09H3E8 | WH-UQ09HE8 | KIT-WQC09H3E8 | 9 | 3ph | T-CAP - Super Quiet |
|15 | E2 CF 0C 55 14 12 D0 0B 15 08 | WH-SDC09H3E5 | WH-UD09HE5 | KIT-WC09H3E5 | 9 | 1 ph | HP |
|16 | E2 CF 0C 43 00 12 D0 0B 15 08 | WH-ADC0309H3E5 | WH-UD09HE5 | KIT-ADC09HE5 | 9 | 1 ph | HP - All-In-One |
|17 | 62 D2 0B 45 54 32 D2 0C 45 55 | WH-ADC0309J3E5 | WH-UD05JE5 | KIT-ADC05JE5 | 5 | 1ph | HP - All-In-One |
|18 | 62 D2 0B 43 54 42 D2 0C 46 55 | WH-SDC0709J3E5 | WH-UD07JE5 | KIT-WC07J3E5 | 7 | 1 ph | HP |
|19 | E2 CF 0C 54 14 12 D0 0B 14 08 | WH-SDC07H3E5-1 | WH-UD07HE5-1 | KIT-WC07H3E5-1 | 7 | 1 ph | HP |
|20 | C2 D3 0B 34 65 B2 D3 0B 95 65 | Monoblock | WH-MDC07J3E5 | Monoblock | 7 | 1ph | HP |
|21 | C2 D3 0B 35 65 B2 D3 0B 96 65 | Monoblock | WH-MDC09J3E5 | Monoblock | 9 | 1ph | HP |
|22 | 62 D2 0B 41 54 32 D2 0C 45 55 | WH-SDC0305J3E5 | WH-UD05JE5 | KIT-WC05J3E5 | 5 | 1ph | HP |
|23 | 32 D4 0B 87 84 73 90 0C 84 84 | Monoblock | WH-MXC09J3E8 | Monoblock | 9 | 3ph | T-CAP |
|24 | 32 D4 0B 88 84 73 90 0C 85 84 | Monoblock | WH-MXC12J9E8 | Monoblock | 12 | 3ph | T-CAP |
|25 | E2 CF 0B 75 09 12 D0 0C 06 11 | WH-ADC1216H6E5 | WH-UD12HE5 | KIT-ADC12HE5 | 12 | 1ph | T-CAP |
|26 | 42 D4 0B 83 71 42 D2 0C 46 55 | WH-ADC0309J3E5C | WH-UD07JE5 | KIT-ADC07JE5C | 7 | 1ph | HP - All-In-One Compact |
|27 | C2 D3 0C 34 65 B2 D3 0B 95 65 | Monoblock | WH-MDC07J3E5 | Monoblock | 7 | 1ph | HP (new version) |
|28 | C2 D3 0C 33 65 B2 D3 0B 94 65 | Monoblock | WH-MDC05J3E5 | Monoblock | 5 | 1ph | HP (new version) |
|29 | E2 CF 0B 83 05 12 D0 0D 92 05 | WH-SQC12H9E8 | WH-UQ12HE8 | KIT-WQC12H9E8 | 12 | 3ph | T-CAP - Super Quiet |
|30 | E2 CF 0C 78 09 12 D0 0B 06 11 | WH-SXC12H6E5 | WH-UX12HE5 | KIT-WXC12H6E5 | 12 | 1ph | T-CAP |
|31 | C2 D3 0C 35 65 B2 D3 0B 96 65 | Monoblock | WH-MDC09J3E5 | Monoblock | 9 | 1ph | HP (new version?) |
|32 | 32 D4 0B 99 77 62 90 0B 01 78 | Monoblock | WH-MXC09J3E5 | Monoblock | 9 | 1ph | T-CAP |
|33 | 42 D4 0B 15 76 12 D0 0B 10 11 | WH-ADC1216H6E5C | WH-UD12HE5 | KIT-ADC12HE5C-CL | 12 | 1ph| HP - All-In-One Compact |
|34 | E2 D5 0C 29 99 83 92 0C 28 98 | WH-ADC0509L3E5 | WH-WDG07LE5 | KIT-ADC07L3E5 | 7 | 1ph | HP - All-In-One L-series |
|35 | E2 CF 0D 85 05 12 D0 0E 94 05 | WH-SXC09H3E8 | WH-UX09HE8 | KIT-WXC09H3E8 | 9 | 3ph | T-CAP - new version |
|36 | E2 D5 0D 36 99 02 D6 0F 67 95 | WH-ADC0309K3E5AN | WH-UDZ07KE5 | KIT-ADC07K3E5AN | 7 | 1ph | HP - All-In-One K-series - AN |
|37 | E2 D5 0B 08 95 02 D6 0E 66 95 | WH-SDC0309K3E5 | WH-UDZ05KE5 | KIT-WC05K3E5 | 5 | 1ph | HP - split K-series |
|38 | E2 D5 0B 34 99 83 92 0C 29 98 | WH-SDC0509L3E5 |  WH-WDG09LE5 | KIT-WC09L3E5 | 9 | 1 ph | HP - split L-series 3kW elec heating |
|39 | E2 CF 0C 89 05 12 D0 0C 98 05 | WH-SDC12H9E8 | WH-UD12HE8 | KIT-WC12H9E8 | 12 | 3ph | HP |
|40 | E2 D5 0B 08 95 02 D6 0E 67 95 | WH-SDC0309K3E5 | WH-UDZ07KE5 | KIT-WC07K3E5 | 7 | 1ph | HP - split K-series |
|41 | E2 CF 0C 74 09 12 D0 0C 96 05 | WH-ADC0916H9E8 | WH-UX16HE8 | KIT-AXC16HE8 | 16 | 3ph | T-CAP - All-In-One |
|42 | E2 CF 0C 74 09 12 D0 0E 95 05 | WH-ADC0912H9E8 | WH-UX12HE8 | KIT-AXC12HE8 | 12 | 3ph | T-CAP - All-In-One |
|43 | 32 D4 0B 89 84 73 90 0C 86 84 | Monoblock | WH-MXC16J9E8 | Monoblock | 16 | 3ph | T-CAP |
|44 | 32 D4 0B 00 78 62 90 0B 02 78 | Monoblock | WH-MXC12J6E5 | Monoblock | 12 | 1ph | T-CAP |
|45 | E2 CF 0B 82 05 12 D0 0D 91 05 | WH-SQC09H3E8 | WH-UQ09HE8 | KIT-WQC09H3E8 | 9 | 3ph | T-CAP - Super Quiet revised model |
|46 | E2 D5 0D 99 94 02 D6 0D 68 95 | WH-ADC0309K3E5 | WH-UDZ09KE5 | KIT-ADC09K3E5 | 9 | 1ph | HP - K-series All-in-One R32 |
|47 | E2 CF 0C 74 09 12 D0 0C 95 05 | WH-ADC0916H9E8 | WH-UX12HE8 | KIT-AXC12HE8 | 12 | 3ph | T-CAP - All-In-One |
|48 | E2 D5 0B 34 99 83 92 0C 28 98 | WH-SDC0509L3E5 |  WH-WDG07LE5 | KIT-WC07L3E5 | 7 | 1 ph | HP - split L-series 3kW elec heating |
|49 | E2 CF 0D 77 09 12 D0 0C 05 11 | WH-SXC09H3E5 | WH-UX09HE5 | KIT-WXC09H3E5 | 9 | 1ph | T-CAP |
|50 | E2 CF 0B 44 34 12 D0 0C 34 35 | Monoblock | WH-MXC12H9E8 | Monoblock | 12 | 3ph | T-CAP |
|51 | E2 D5 0C 67 00 83 92 0C 27 98 | WH-ADC0509L3E5AN | WH-WDG05LE5 | KIT-ADC05L3E5AN | 5 | 1ph | HP - split L-series 3kW elec heating - AN |
|52 | E2 D5 0B 34 99 83 92 0C 27 98 | WH-SDC0509L3E5 | WH-WDG05LE5 | KIT-WC05L3E5 | 5 | 1ph | HP - split L-series 3kW elec heating |
|53 | 42 D4 0B 83 71 32 D2 0C 44 55 | WH-ADC0309J3E5C | WH-UD03JE5 | KIT-ADC03JE5C-S | 3.2 | 1ph | HP - All-In-One Compact |
|54 | E2 CF 0C 74 09 12 D0 0E 94 05 | WH-ADC0916H9E8 | WH-UX09HE8 | KIT-AXC9HE8 | 9 | 3ph | T-CAP - All-In-One |
|55 | 12 D7 0D 98 11 33 94 0C 83 10 | WH-ADC0316M9E8AN2 | WH-WXG09ME8 | Monoblock | 9 | 2ph | T-CAP - M-series DHW 185l |
|56 | E2 D5 0B 08 95 02 D6 0F 67 95 | WH-SDC0309K3E5 | WH-UDZ07KE5 | KIT-SDC07KE5 | 7 | 1ph | HP - split K-series (sold in Poland) |
|57 | E2 D5 0D 36 99 02 D6 10 66 95 | WH-ADC0309K3E5AN | WH-UDZ05KE5 | KIT-ADC05K3E5AN | 5 | 1ph | HP - All-In-One K-series - AN |
|58 | E2 D5 0F 99 94 02 D6 10 68 95 | WH-ADC0309K3E5 | WH-UDZ09KE5 | KIT-ADC09K3E5 | 9 | 1ph | HP - split K-series 9KW |
|59 | E2 D5 0C 67 00 83 92 0C 28 98 | WH-ADC0509L3E5AN | WH-WDG07LE5 | KIT-ADC07L3E5AN | 7 | 1ph | HP - All-In-One L-series - AN |
|60 | E2 CF 0D 86 05 12 D0 0E 95 05 | WH-SXC12H9E8 | WH-UX12HE8 | KIT-WXC12H9E8 | 12 | 3ph | T-CAP |
|61 | 12 D7 0C 98 14 35 94 0D 83 10 | WH-SDC0316M9E8 | WH-WXG09ME8 | KIT-SDCWXG09 | 9 | 3ph | T-CAP - M-series | 
|62 | E2 D5 0C 08 95 02 D6 10 67 95 | WH-SDC0309K3E5 | WH-UDZ07KE5 | KIT-SDC07KE5 | 7 | 1ph | HP - split K-series 3kW elec heating |
|63 | C2 D3 0D 35 65 B2 D3 0B 96 65 | Monoblock | WH-MDC09J3E5-1 | Monoblock | 9 | 1ph | HP |
|64 | E2 D5 0C 29 99 83 92 0C 29 98 | WH-ADC0509L3E5 | WH-WDG09LE5 | KIT-ADC09L3E5 | 9 | 1ph | HP - All-In-One L-series |
|65 | 62 D2 0B 45 54 42 D2 0B 72 66 | WH-ADC0309J3E5 | WH-UD09JE5-1 | KIT-ADC09JE5 | 9 | 1ph | HP - All-In-One J-series |
|66 | E2 CF 0B 82 09 12 D0 0B 10 11 | WH-SDC12H6E5 | WH-UD12HE5 | KIT-WC012H6E5 | 12 | 1ph | HP |
|67 | 42 D4 0B 83 71 32 D2 0C 45 55 | WH-ADC0309J3E5C | WH-UD05JE5 | KIT-ADC05JE5C | 5 | 1ph | HP - All-In-One J-series |
|68 | 12 D7 0B 47 19 37 94 0E 83 10 | WH-ADC0316M9E83 | WH-WXG09ME8 | Monoblock | 9 | 3ph | T-CAP - M-series DHW 260l |
|69 | E2 D5 0D 00 09 43 D6 0E 07 09 | WH-SDC09K3E8 | WH-UDZ09KE8 | KIT-WC09K3E8 | 9 | 3ph | HP |
|70 | E2 CF 0B 84 05 12 D0 0B 93 05 | WH-SQC16H9E8 | WH-UQ16HE8 | KIT-WQC16H9E8 | 16 | 3ph | T-CAP - Bi-bloc H Serie |
|71 | E2 D5 0B 35 99 83 92 0D 29 98 | WH-SDC0509L6E5 | WH-WDG09LE5 | KIT-WC09L6E5 | 9 | 1 ph | HP - split L-series 6kW elec heating |
|72 | 12 D7 0B 98 14 35 94 0D 83 10 | WH-SDC0316M9E8 | WH-WXG09ME8 | Monoblock | 9 | 3ph | T-CAP - M-series  |
|73 | E2 CF 0C 77 09 12 D0 0B 05 11 | WH-SXC09H3E5 | WH-UX09HE5 | KIT-WXC09H3E5 | 9 | 1ph | T-CAP |

All bytes are used for Heat Pump model identification in the code.

Note: These are the heat pump types that users have verified and reported back the model. HeishaMon should work with all H and J generation Aquarea heat pumps.
