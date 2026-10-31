# rot04 Stock vs Custom Trace
Custom: `logs/rot04_mode_range_trace_fast_20260507_091443.log`
Stock: `logs/rot04_stock_mode_range_trace_fast_20260507_092135.log`
## prime
### stock
rows=4 tw=['E000'] sfl=['00'] h=['00'] gains=['40C5DC48', '40C617EC', '427A5CAC']
state changes: t mode range r1f r20 tw sfl h
- 0.000 03 01 04 62 E000 00 00
- 0.526 04 00 05 F6 E000 00 00
- 1.066 05 00 04 62 E000 00 00
- 1.562 03 00 04 62 E000 00 00
### custom
rows=4 tw=['E000'] sfl=['00'] h=['00', '01', '03', '04'] gains=['005E58E4', '005E7554', '03ADF630', '03BB0E24']
state changes: t mode range r1f r20 tw sfl h
- 0.000 03 01 04 62 E000 00 00
- 0.512 04 00 05 F6 E000 00 04
- 1.027 05 00 04 62 E000 00 03
- 1.534 03 00 04 62 E000 00 01

## transition_1
### stock
rows=52 tw=['8000', 'E000'] sfl=['00', '20'] h=['00'] gains=['4276EDFA']
state changes: t mode range r1f r20 tw sfl h
- 0.000 04 00 05 F6 E000 00 00
- 1.305 04 00 05 F6 8000 20 00
- 3.005 04 00 05 F6 E000 00 00
- 4.505 04 00 05 F6 8000 20 00
### custom
rows=52 tw=['E000'] sfl=['00'] h=['00', '01', '02', '03', '04'] gains=['03ADF630']
state changes: t mode range r1f r20 tw sfl h
- 0.000 04 00 05 F6 E000 00 04
- 0.705 04 00 05 F6 E000 00 03
- 1.304 04 00 05 F6 E000 00 02
- 1.806 04 00 05 F6 E000 00 01
- 2.404 04 00 05 F6 E000 00 00

## transition_2
### stock
rows=52 tw=['8000'] sfl=['20'] h=['00'] gains=['40C617EC']
state changes: t mode range r1f r20 tw sfl h
- 0.000 05 00 04 62 8000 20 00
### custom
rows=52 tw=['E000'] sfl=['00'] h=['03'] gains=['005E7554']
state changes: t mode range r1f r20 tw sfl h
- 0.000 05 00 04 62 E000 00 03

## transition_3
### stock
rows=52 tw=['8000'] sfl=['20'] h=['00'] gains=['40C5DC48', '427A5CAC']
state changes: t mode range r1f r20 tw sfl h
- 0.000 03 00 04 62 8000 20 00
- 0.703 03 01 04 62 8000 20 00
- 2.105 03 01 04 62 8000 20 
- 2.204 03 01 04 62 8000 20 00
- 2.904 03 01 04 62 8000 20 
- 3.005 03 01 04 62 8000 20 00
- 3.705 03 01 04 62 8000 20 
- 3.805 03 01 04 62 8000 20 00
### custom
rows=52 tw=['E000'] sfl=['00'] h=['00', '01'] gains=['005E58E4', '03BB0E24']
state changes: t mode range r1f r20 tw sfl h
- 0.000 03 00 04 62 E000 00 01
- 0.904 03 00 04 62 E000 00 00
- 1.005 03 01 04 62 E000 00 01
- 1.505 03 01 04 62 E000 00 00

## Key differences
- transition_1: Stock tw=['8000', 'E000'] sfl=['00', '20'] h=['00'] ranges=[(0.0, '04', '00')]; Custom tw=['E000'] sfl=['00'] h=['00', '01', '02', '03', '04'] ranges=[(0.0, '04', '00')]
- transition_2: Stock tw=['8000'] sfl=['20'] h=['00'] ranges=[(0.0, '05', '00')]; Custom tw=['E000'] sfl=['00'] h=['03'] ranges=[(0.0, '05', '00')]
- transition_3: Stock tw=['8000'] sfl=['20'] h=['00'] ranges=[(0.0, '03', '00'), (0.703, '03', '01')]; Custom tw=['E000'] sfl=['00'] h=['00', '01'] ranges=[(0.0, '03', '00'), (1.005, '03', '01')]
