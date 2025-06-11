# Galaxy Watch 4 40mm (SM-R860)
## A sort-of reverse engineering of the SM-R860 and custom firmware

# NOTICE
This project has been adandoned for now due to me not being able to achieve over 10 hours of battery life. <br>
This is probably due to battery degredation, but I have not had the chance to properly test that theory. <br>
On top of that, I also (thanks Samsung) do not have a copy of the original firmware to test the original Android battery life. <br>
The watch software itself works wonderfully and is very efficient (1% on both cores), and honestly I am sad that I cannot use it due to the battery life.

# Sub-Projects:
## WatchApp
- A new watch interface that runs directly on the framebuffer of the downstream kernel (Includes a charging screen) <br>
This code is licensed under GPLv2

## Information
- In depth information about the SM-R860

## ModifiedKernel
- The Modified Kernel is from Samsung Open Source (Original file is SM-R860_NA_Opensource_R860XXU1HWH3_full.zip), with some modifications made so certain subsystems / drivers work within a standard linux environment

## Development
- Bits of code that aren't ready to be merged into the main program
