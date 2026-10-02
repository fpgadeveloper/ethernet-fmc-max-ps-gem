# Requirements

In order to build and test this design on hardware, you will need the following:

* Vivado 2025.2
* Vitis 2025.2 (for the standalone lwIP echo-server flow, and for the Yocto flow, which uses
  its `sdtgen` tool)
* For embedded Linux, one of:
  * PetaLinux Tools 2025.2, or
  * the Yocto / AMD EDF flow: [Google's repo tool](https://gerrit.googlesource.com/git-repo/)
    and the usual Yocto host packages (see [Yocto](yocto.md#requirements))
* A Linux machine for the PetaLinux and Yocto builds (the Vivado and Vitis builds also run on
  Windows)
* [Ethernet FMC Max]
* One of the supported carrier boards listed below

To run and test the design you will also need:

* An SD card for the PetaLinux and Yocto images, and an SD card reader on your PC. The Yocto
  disk image is slightly larger than 8 GiB, so use a 16 GB or larger card for Yocto.
* The board's USB-UART cable (console) and, for the standalone application or JTAG boot, the
  board's USB-JTAG connection
* Ethernet cables (Cat5e or better) and a link partner for each port you want to test: a PC
  with a gigabit Ethernet port, or a network switch/router. A DHCP server on the network is
  convenient (the Yocto image and the echo server request an address by DHCP) but not
  required.
* To measure throughput: a PC on the same network running `iperf3`

## List of supported boards

{% set unique_boards = {} %}
{% for design in data.designs %}
	{% if design.publish %}
	    {% if design.board not in unique_boards %}
	        {% set _ = unique_boards.update({design.board: {"group": design.group, "link": design.link, "connectors": []}}) %}
	    {% endif %}
	    {% if design.connector not in unique_boards[design.board]["connectors"] %}
	    	{% set _ = unique_boards[design.board]["connectors"].append(design.connector) %}
	    {% endif %}
	{% endif %}
{% endfor %}

{% for group in data.groups %}
    {% set boards_in_group = [] %}
    {% for name, board in unique_boards.items() %}
        {% if board.group == group.label %}
            {% set _ = boards_in_group.append(board) %}
        {% endif %}
    {% endfor %}

    {% if boards_in_group | length > 0 %}
### {{ group.name }} boards

| Carrier board        | Supported FMC connector(s)    |
|---------------------|--------------|
{% for name,board in unique_boards.items() %}{% if board.group == group.label %}| [{{ name }}]({{ board.link }}) | {% for connector in board.connectors %}{{ connector }} {% endfor %} |
{% endif %}{% endfor %}
{% endif %}
{% endfor %}

For list of the target designs showing the number of ports supported, refer to the build instructions.

[Ethernet FMC Max]: https://docs.opsero.com/op080/datasheet/overview/
