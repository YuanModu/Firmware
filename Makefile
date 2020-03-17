##############################################################################
# Multi-project makefile rules
#

all:
	@echo
	@echo === Building for STM32F429-Nucleo ===============================
	+@make --no-print-directory -f make/stm32f429_nucleo.make all
	@echo ====================================================================
	@echo

clean:
	@echo
	+@make --no-print-directory -f make/stm32f429_nucleo.make clean
	@echo

#
##############################################################################
