# esp32_sampler_playground
test project for sampler functionality on an esp32

This version uses SPI for the SD Card

It streams backing tracks from the SD card as mp3 files and can also play polyphonic samples from ram. 

needs good powermanagement to run properly otherwise expect clicks and pops. Runnning an external power works well, but I am finding that running it from the 5vo on the esp into vin also gives good quality

if running on external power then you have to be really careful to manage the usb power vs external power to not fry something. 

Ok after loads of messing around with libraries, here is a simple wav based solution that works without a library and has 8 note polyphony