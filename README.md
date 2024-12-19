Different captouch implementations in `src/cap_touch`. The only two implemented to a working state is `ct_adc_charge_share.c` and `ct_current_oscillate.c`. Select between them with
```
CONFIG_CAP_TOUCH_COMP_CURRENT=y
```
or
```
CONFIG_CAP_TOUCH_ADC_CHARGE_SHARE=y
```
in `prj.conf`

Compile with:
- `prj.conf`
- `debug.conf` or `debug.conf` as overlay
- desired log method, from `overlays/` as overlay (if debug)

Make sure to configure `hardware_spec.h` to your specific setup
