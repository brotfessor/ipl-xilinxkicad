# ipl-xilinxkicad
Adds the internal package delays for a Xilinx FPGA to a kicad
footprint for proper trace length matching.

## Compiling
You need the [sfsexp](https://github.com/mjsottile/sfsexp) library for this:

    git clone https://github.com/mjsottile/sfsexp
	cd sfsexp
	autoreconf --install
	./configure
	make
	(sudo) make install
	
After that you can compile this tool:

    git clone https://github.com/brotfessor/ipl-xilinxkicad
	cd ipl-xilinxkicad
    make
	(sudo) make install
	
## Usage
You need to export the pin information from vivado via

    File -> Export -> Export I/O ports
	
Then you have to find out where the footprint file for your package is
located. This is usually somwhere in `/usr/share/kicad/modules`. Then
use the tool for example like that:

    ipl-xilinxkicad \
	-i /usr/share/kicad/modules/Package_BGA.pretty/Xilinx_FGG484.kicad_mod \
	-o /path/to/your/project/Xilinx_FGG484_tracelength.kicad_mod \
	-c /path/to/your/pinout/file.csv \
	-e 4.5
	
The value behind e is the Epsilon_r value of your FR4 material.
By default it prints information about every pin, use `-q` to silence it.
If no output file is specified, it gets printed to stdout.

## Function
Vivado outputs a package delay in picoseconds, more specifically an
upper and a lower limit to account for variations in the production
process. Kicad doesn't know about timing, it only cares about trace
length so we convert the package delay in the equivalent trace length
that would have the same effect on a PCB. This depends on the
dielectric constant which is why you have to enter it.

The package delay depends (to a certain degree) on the speed and
I/O standard used on a pin. So it is advisable to do this after all
signals have been assigned a location and an I/O standard.
