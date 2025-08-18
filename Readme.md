
## Steps to build and flash
```sh

west build -b promicro_nrf52840 --sysbuild --pristine -- -DBOARD_ROOT=$PWD

west flash --runner pyocd
```


