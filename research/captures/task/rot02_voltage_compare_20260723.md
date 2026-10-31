# rotary capture comparison

stock=logs/rot02_stock_voltage_reference_20260723.log
custom=logs/rot02_custom_voltage_timing_fixed_20260723.log

| section | stock mode/range | custom mode/range | stock frontend | custom frontend | value | lcd | differences |
|---|---:|---:|---|---|---|---|---|
| rot02_v rot=02 | ?/? | ?/? | `p10 r1f=- r20=- live r1f=- r20=- cfg=-` | `p10 r1f=- r20=- live r1f=- r20=- cfg=-` | `- / -` | same | same |
| rot02_v rot=02 / rot02_v step=0 mode=01 | 01/00 | 01/00 | `p10 r1f=04 r20=02 live r1f=- r20=- cfg=0200138A0540004D31012200009028A080C70020` | `p10 r1f=04 r20=02 live r1f=- r20=- cfg=0200138A0540004D31012200009028A080C70020` | `FFFFFFED / 0000000E` | value-only | val: FFFFFFED != 0000000E; disp: 0000000A != FFFFFF6A; raw: 000002EA != FFFFDCB7; av: FFFFFBB1 != 0000000E; b0: 00000000 != 80000000; b1: 00000000 != 80000000 |
| rot02_v rot=02 / rot02_v step=1 mode=02 | 02/00 | 02/00 | `p10 r1f=05 r20=F6 live r1f=- r20=- cfg=F6DD0283F541024D31010000000988B080C0382C` | `p10 r1f=05 r20=F6 live r1f=- r20=- cfg=F6DD0283F541024D31010000000988B080C0382C` | `000043D8 / 00004008` | value-only | flags: 00 != 03; val: 000043D8 != 00004008; disp: 000043DA != 00004014; raw: 0010B690 != 000FCAE0; av: 0010B643 != 00004008; b0: 00000000 != 80000000; b1: 00000000 != 80000000 |
| rot02_v rot=02 / rot02_v step=2 mode=02 | 02/00 | 02/00 | `p10 r1f=04 r20=02 live r1f=- r20=- cfg=0200138A0540004D31012200009028A080C70020` | `p10 r1f=04 r20=02 live r1f=- r20=- cfg=F6DD0283F541024D31010000000988B080C0382C` | `000001AA / 00000011` | value-only | flags: 00 != 02; sfl: 03 != 01; val: 000001AA != 00000011; disp: 000043CC != 00003FFF; zero: 00000087 != 000045AF; raw: 00000A38 != 00004A1C; av: 0000661A != 0000400C; b0: 00000000 != 80000000; b1: 00000000 != 80000000; ctrl mask: 04 != 05; frontend cfg differs |
