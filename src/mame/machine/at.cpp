// license:BSD-3-Clause
// copyright-holders:Wilbert Pol, Miodrag Milanovic, Carl
/***************************************************************************

    IBM AT Compatibles

***************************************************************************/

#include "emu.h"
#include "machine/at.h"
#include "cpu/i86/i286.h"
#include "cpu/i386/i386.h"
#include "machine/at_keybc.h"
#include "bus/pc_kbd/pc_kbdc.h"
#include "softlist_dev.h"
#include "speaker.h"

#define LOG_PORT80  0

DEFINE_DEVICE_TYPE(AT_MB, at_mb_device, "at_mb", "PC/AT Motherboard")

at_mb_device::at_mb_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, AT_MB, tag, owner, clock),
	m_maincpu(*this, ":maincpu"),
	m_isabus(*this, "isabus"),
	m_pic8259_slave(*this, "pic8259_slave"),
	m_dma8237_1(*this, "dma8237_1"),
	m_dma8237_2(*this, "dma8237_2"),
	m_pit8254(*this, "pit8254"),
	m_speaker(*this, "speaker"),
	m_mc146818(*this, "rtc")
{
}

void at_mb_device::device_reset()
{
	m_at_spkrdata = 0;
	m_pit_out2 = 1;
	m_dma_channel = -1;
	m_cur_eop = false;
}

void at_mb_device::device_start()
{
	if(!strncmp(m_maincpu->shortname(), "i80286", 6))
		downcast<i80286_cpu_device *>(m_maincpu.target())->set_a20_callback(i80286_cpu_device::a20_cb(&at_mb_device::a20_286, this));
}

MACHINE_CONFIG_START(at_mb_device::at_softlists)
	/* software lists */
	MCFG_SOFTWARE_LIST_ADD("pc_disk_list","ibm5150")
	MCFG_SOFTWARE_LIST_ADD("at_disk_list","ibm5170")
	MCFG_SOFTWARE_LIST_ADD("at_cdrom_list","ibm5170_cdrom")
MACHINE_CONFIG_END

MACHINE_CONFIG_START(at_mb_device::device_add_mconfig)
	MCFG_DEVICE_ADD("pit8254", PIT8254, 0)
	MCFG_PIT8253_CLK0(4772720/4) /* heartbeat IRQ */
	MCFG_PIT8253_OUT0_HANDLER(WRITELINE("pic8259_master", pic8259_device, ir0_w))
	MCFG_PIT8253_CLK1(4772720/4) /* dram refresh */
	MCFG_PIT8253_CLK2(4772720/4) /* pio port c pin 4, and speaker polling enough */
	MCFG_PIT8253_OUT2_HANDLER(WRITELINE(*this, at_mb_device, pit8254_out2_changed))

	MCFG_DEVICE_ADD("dma8237_1", AM9517A, 14.318181_MHz_XTAL / 3)
	MCFG_I8237_OUT_HREQ_CB(WRITELINE("dma8237_2", am9517a_device, dreq0_w))
	MCFG_I8237_OUT_EOP_CB(WRITELINE(*this, at_mb_device, dma8237_out_eop))
	MCFG_I8237_IN_MEMR_CB(READ8(*this, at_mb_device, dma_read_byte))
	MCFG_I8237_OUT_MEMW_CB(WRITE8(*this, at_mb_device, dma_write_byte))
	MCFG_I8237_IN_IOR_0_CB(READ8(*this, at_mb_device, dma8237_0_dack_r))
	MCFG_I8237_IN_IOR_1_CB(READ8(*this, at_mb_device, dma8237_1_dack_r))
	MCFG_I8237_IN_IOR_2_CB(READ8(*this, at_mb_device, dma8237_2_dack_r))
	MCFG_I8237_IN_IOR_3_CB(READ8(*this, at_mb_device, dma8237_3_dack_r))
	MCFG_I8237_OUT_IOW_0_CB(WRITE8(*this, at_mb_device, dma8237_0_dack_w))
	MCFG_I8237_OUT_IOW_1_CB(WRITE8(*this, at_mb_device, dma8237_1_dack_w))
	MCFG_I8237_OUT_IOW_2_CB(WRITE8(*this, at_mb_device, dma8237_2_dack_w))
	MCFG_I8237_OUT_IOW_3_CB(WRITE8(*this, at_mb_device, dma8237_3_dack_w))
	MCFG_I8237_OUT_DACK_0_CB(WRITELINE(*this, at_mb_device, dack0_w))
	MCFG_I8237_OUT_DACK_1_CB(WRITELINE(*this, at_mb_device, dack1_w))
	MCFG_I8237_OUT_DACK_2_CB(WRITELINE(*this, at_mb_device, dack2_w))
	MCFG_I8237_OUT_DACK_3_CB(WRITELINE(*this, at_mb_device, dack3_w))

	MCFG_DEVICE_ADD( "dma8237_2", AM9517A, 14.318181_MHz_XTAL / 3)
	MCFG_I8237_OUT_HREQ_CB(WRITELINE(*this, at_mb_device, dma_hrq_changed))
	MCFG_I8237_OUT_EOP_CB(WRITELINE(*this, at_mb_device, dma8237_2_out_eop))
	MCFG_I8237_IN_MEMR_CB(READ8(*this, at_mb_device, dma_read_word))
	MCFG_I8237_OUT_MEMW_CB(WRITE8(*this, at_mb_device, dma_write_word))
	MCFG_I8237_IN_IOR_1_CB(READ8(*this, at_mb_device, dma8237_5_dack_r))
	MCFG_I8237_IN_IOR_2_CB(READ8(*this, at_mb_device, dma8237_6_dack_r))
	MCFG_I8237_IN_IOR_3_CB(READ8(*this, at_mb_device, dma8237_7_dack_r))
	MCFG_I8237_OUT_IOW_1_CB(WRITE8(*this, at_mb_device, dma8237_5_dack_w))
	MCFG_I8237_OUT_IOW_2_CB(WRITE8(*this, at_mb_device, dma8237_6_dack_w))
	MCFG_I8237_OUT_IOW_3_CB(WRITE8(*this, at_mb_device, dma8237_7_dack_w))
	MCFG_I8237_OUT_DACK_0_CB(WRITELINE(*this, at_mb_device, dack4_w))
	MCFG_I8237_OUT_DACK_1_CB(WRITELINE(*this, at_mb_device, dack5_w))
	MCFG_I8237_OUT_DACK_2_CB(WRITELINE(*this, at_mb_device, dack6_w))
	MCFG_I8237_OUT_DACK_3_CB(WRITELINE(*this, at_mb_device, dack7_w))

	MCFG_DEVICE_ADD("pic8259_master", PIC8259, 0)
	MCFG_PIC8259_OUT_INT_CB(INPUTLINE(":maincpu", 0))
	MCFG_PIC8259_IN_SP_CB(VCC)
	MCFG_PIC8259_CASCADE_ACK_CB(READ8(*this, at_mb_device, get_slave_ack))

	MCFG_DEVICE_ADD("pic8259_slave", PIC8259, 0)
	MCFG_PIC8259_OUT_INT_CB(WRITELINE("pic8259_master", pic8259_device, ir2_w))
	MCFG_PIC8259_IN_SP_CB(GND)

	MCFG_DEVICE_ADD("isabus", ISA16, 0)
	MCFG_ISA16_CPU(":maincpu")
	MCFG_ISA_OUT_IRQ2_CB(WRITELINE("pic8259_slave",  pic8259_device, ir2_w)) // in place of irq 2 on at irq 9 is used
	MCFG_ISA_OUT_IRQ3_CB(WRITELINE("pic8259_master", pic8259_device, ir3_w))
	MCFG_ISA_OUT_IRQ4_CB(WRITELINE("pic8259_master", pic8259_device, ir4_w))
	MCFG_ISA_OUT_IRQ5_CB(WRITELINE("pic8259_master", pic8259_device, ir5_w))
	MCFG_ISA_OUT_IRQ6_CB(WRITELINE("pic8259_master", pic8259_device, ir6_w))
	MCFG_ISA_OUT_IRQ7_CB(WRITELINE("pic8259_master", pic8259_device, ir7_w))
	MCFG_ISA_OUT_IRQ10_CB(WRITELINE("pic8259_slave", pic8259_device, ir3_w))
	MCFG_ISA_OUT_IRQ11_CB(WRITELINE("pic8259_slave", pic8259_device, ir4_w))
	MCFG_ISA_OUT_IRQ12_CB(WRITELINE("pic8259_slave", pic8259_device, ir5_w))
	MCFG_ISA_OUT_IRQ14_CB(WRITELINE("pic8259_slave", pic8259_device, ir6_w))
	MCFG_ISA_OUT_IRQ15_CB(WRITELINE("pic8259_slave", pic8259_device, ir7_w))
	MCFG_ISA_OUT_DRQ0_CB(WRITELINE("dma8237_1", am9517a_device, dreq0_w))
	MCFG_ISA_OUT_DRQ1_CB(WRITELINE("dma8237_1", am9517a_device, dreq1_w))
	MCFG_ISA_OUT_DRQ2_CB(WRITELINE("dma8237_1", am9517a_device, dreq2_w))
	MCFG_ISA_OUT_DRQ3_CB(WRITELINE("dma8237_1", am9517a_device, dreq3_w))
	MCFG_ISA_OUT_DRQ5_CB(WRITELINE("dma8237_2", am9517a_device, dreq1_w))
	MCFG_ISA_OUT_DRQ6_CB(WRITELINE("dma8237_2", am9517a_device, dreq2_w))
	MCFG_ISA_OUT_DRQ7_CB(WRITELINE("dma8237_2", am9517a_device, dreq3_w))

	MCFG_DEVICE_ADD("rtc", MC146818, 32.768_kHz_XTAL)
	MCFG_MC146818_IRQ_HANDLER(WRITELINE("pic8259_slave", pic8259_device, ir0_w)) MCFG_DEVCB_INVERT
	MCFG_MC146818_CENTURY_INDEX(0x32)

	/* sound hardware */
	SPEAKER(config, "mono").front_center();
	MCFG_DEVICE_ADD("speaker", SPEAKER_SOUND)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)

	MCFG_DEVICE_ADD("keybc", AT_KEYBOARD_CONTROLLER, 12_MHz_XTAL)
	MCFG_AT_KEYBOARD_CONTROLLER_SYSTEM_RESET_CB(INPUTLINE(":maincpu", INPUT_LINE_RESET))
	MCFG_AT_KEYBOARD_CONTROLLER_GATE_A20_CB(INPUTLINE(":maincpu", INPUT_LINE_A20))
	MCFG_AT_KEYBOARD_CONTROLLER_INPUT_BUFFER_FULL_CB(WRITELINE("pic8259_master", pic8259_device, ir1_w))
	MCFG_AT_KEYBOARD_CONTROLLER_KEYBOARD_CLOCK_CB(WRITELINE("pc_kbdc", pc_kbdc_device, clock_write_from_mb))
	MCFG_AT_KEYBOARD_CONTROLLER_KEYBOARD_DATA_CB(WRITELINE("pc_kbdc", pc_kbdc_device, data_write_from_mb))
	MCFG_DEVICE_ADD("pc_kbdc", PC_KBDC, 0)
	MCFG_PC_KBDC_OUT_CLOCK_CB(WRITELINE("keybc", at_keyboard_controller_device, keyboard_clock_w))
	MCFG_PC_KBDC_OUT_DATA_CB(WRITELINE("keybc", at_keyboard_controller_device, keyboard_data_w))
MACHINE_CONFIG_END


void at_mb_device::map(address_map &map)
{
	map(0x0000, 0x001f).rw("dma8237_1", FUNC(am9517a_device::read), FUNC(am9517a_device::write)).umask16(0xffff);
	map(0x0020, 0x003f).rw("pic8259_master", FUNC(pic8259_device::read), FUNC(pic8259_device::write)).umask16(0xffff);
	map(0x0040, 0x005f).rw("pit8254", FUNC(pit8254_device::read), FUNC(pit8254_device::write)).umask16(0xffff);
	map(0x0061, 0x0061).rw(FUNC(at_mb_device::portb_r), FUNC(at_mb_device::portb_w));
	map(0x0060, 0x0060).rw("keybc", FUNC(at_keyboard_controller_device::data_r), FUNC(at_keyboard_controller_device::data_w));
	map(0x0064, 0x0064).rw("keybc", FUNC(at_keyboard_controller_device::status_r), FUNC(at_keyboard_controller_device::command_w));
	map(0x0070, 0x007f).r("rtc", FUNC(mc146818_device::read)).umask16(0xffff).w(FUNC(at_mb_device::write_rtc)).umask16(0xffff);
	map(0x0080, 0x009f).rw(FUNC(at_mb_device::page8_r), FUNC(at_mb_device::page8_w)).umask16(0xffff);
	map(0x00a0, 0x00bf).rw("pic8259_slave", FUNC(pic8259_device::read), FUNC(pic8259_device::write)).umask16(0xffff);
	map(0x00c0, 0x00df).rw("dma8237_2", FUNC(am9517a_device::read), FUNC(am9517a_device::write)).umask16(0x00ff);
}

/*************************************************************
 *
 * pic8259 configuration
 *
 *************************************************************/
READ8_MEMBER( at_mb_device::get_slave_ack )
{
	if (offset==2) // IRQ = 2
		return m_pic8259_slave->acknowledge();

	return 0x00;
}

/*************************************************************************
 *
 *      PC Speaker related
 *
 *************************************************************************/

void at_mb_device::speaker_set_spkrdata(uint8_t data)
{
	m_at_spkrdata = data ? 1 : 0;
	m_speaker->level_w(m_at_spkrdata & m_pit_out2);
}



/*************************************************************
 *
 * pit8254 configuration
 *
 *************************************************************/

WRITE_LINE_MEMBER( at_mb_device::pit8254_out2_changed )
{
	m_pit_out2 = state ? 1 : 0;
	m_speaker->level_w(m_at_spkrdata & m_pit_out2);
}


/*************************************************************************
 *
 *      PC DMA stuff
 *
 *************************************************************************/

READ8_MEMBER( at_mb_device::page8_r )
{
	uint8_t data = m_at_pages[offset % 0x10];

	switch(offset % 8)
	{
	case 1:
		data = m_dma_offset[BIT(offset, 3)][2];
		break;
	case 2:
		data = m_dma_offset[BIT(offset, 3)][3];
		break;
	case 3:
		data = m_dma_offset[BIT(offset, 3)][1];
		break;
	case 7:
		data = m_dma_offset[BIT(offset, 3)][0];
		break;
	}
	return data;
}


WRITE8_MEMBER( at_mb_device::page8_w )
{
	m_at_pages[offset % 0x10] = data;

	if (LOG_PORT80 && (offset == 0))
	{
		logerror(" at_page8_w(): Port 80h <== 0x%02x (PC=0x%08x)\n", data,
							(unsigned) m_maincpu->pc());
	}

	switch(offset % 8)
	{
	case 1:
		m_dma_offset[BIT(offset, 3)][2] = data;
		break;
	case 2:
		m_dma_offset[BIT(offset, 3)][3] = data;
		break;
	case 3:
		m_dma_offset[BIT(offset, 3)][1] = data;
		break;
	case 7:
		m_dma_offset[BIT(offset, 3)][0] = data;
		break;
	}
}


WRITE_LINE_MEMBER( at_mb_device::dma_hrq_changed )
{
	m_maincpu->set_input_line(INPUT_LINE_HALT, state ? ASSERT_LINE : CLEAR_LINE);

	/* Assert HLDA */
	m_dma8237_2->hack_w(state);
}

READ8_MEMBER(at_mb_device::dma_read_byte)
{
	address_space& prog_space = m_maincpu->space(AS_PROGRAM); // get the right address space
	if(m_dma_channel == -1)
		return 0xff;
	uint8_t result;
	offs_t page_offset = ((offs_t) m_dma_offset[0][m_dma_channel]) << 16;

	result = prog_space.read_byte(page_offset + offset);
	return result;
}


WRITE8_MEMBER(at_mb_device::dma_write_byte)
{
	address_space& prog_space = m_maincpu->space(AS_PROGRAM); // get the right address space
	if(m_dma_channel == -1)
		return;
	offs_t page_offset = ((offs_t) m_dma_offset[0][m_dma_channel]) << 16;

	prog_space.write_byte(page_offset + offset, data);
}


READ8_MEMBER(at_mb_device::dma_read_word)
{
	address_space& prog_space = m_maincpu->space(AS_PROGRAM); // get the right address space
	if(m_dma_channel == -1)
		return 0xff;
	uint16_t result;
	offs_t page_offset = ((offs_t) m_dma_offset[1][m_dma_channel & 3]) << 16;

	result = prog_space.read_word((page_offset & 0xfe0000) | (offset << 1));
	m_dma_high_byte = result & 0xFF00;

	return result & 0xFF;
}


WRITE8_MEMBER(at_mb_device::dma_write_word)
{
	address_space& prog_space = m_maincpu->space(AS_PROGRAM); // get the right address space
	if(m_dma_channel == -1)
		return;
	offs_t page_offset = ((offs_t) m_dma_offset[1][m_dma_channel & 3]) << 16;

	prog_space.write_word((page_offset & 0xfe0000) | (offset << 1), m_dma_high_byte | data);
}

READ8_MEMBER( at_mb_device::dma8237_0_dack_r ) { return m_isabus->dack_r(0); }
READ8_MEMBER( at_mb_device::dma8237_1_dack_r ) { return m_isabus->dack_r(1); }
READ8_MEMBER( at_mb_device::dma8237_2_dack_r ) { return m_isabus->dack_r(2); }
READ8_MEMBER( at_mb_device::dma8237_3_dack_r ) { return m_isabus->dack_r(3); }
READ8_MEMBER( at_mb_device::dma8237_5_dack_r ) { uint16_t ret = m_isabus->dack16_r(5); m_dma_high_byte = ret & 0xff00; return ret; }
READ8_MEMBER( at_mb_device::dma8237_6_dack_r ) { uint16_t ret = m_isabus->dack16_r(6); m_dma_high_byte = ret & 0xff00; return ret; }
READ8_MEMBER( at_mb_device::dma8237_7_dack_r ) { uint16_t ret = m_isabus->dack16_r(7); m_dma_high_byte = ret & 0xff00; return ret; }


WRITE8_MEMBER( at_mb_device::dma8237_0_dack_w ){ m_isabus->dack_w(0, data); }
WRITE8_MEMBER( at_mb_device::dma8237_1_dack_w ){ m_isabus->dack_w(1, data); }
WRITE8_MEMBER( at_mb_device::dma8237_2_dack_w ){ m_isabus->dack_w(2, data); }
WRITE8_MEMBER( at_mb_device::dma8237_3_dack_w ){ m_isabus->dack_w(3, data); }
WRITE8_MEMBER( at_mb_device::dma8237_5_dack_w ){ m_isabus->dack16_w(5, m_dma_high_byte | data); }
WRITE8_MEMBER( at_mb_device::dma8237_6_dack_w ){ m_isabus->dack16_w(6, m_dma_high_byte | data); }
WRITE8_MEMBER( at_mb_device::dma8237_7_dack_w ){ m_isabus->dack16_w(7, m_dma_high_byte | data); }

WRITE_LINE_MEMBER( at_mb_device::dma8237_out_eop )
{
	m_cur_eop = state == ASSERT_LINE;
	if(m_dma_channel != -1)
		m_isabus->eop_w(m_dma_channel, m_cur_eop ? ASSERT_LINE : CLEAR_LINE );
}

WRITE_LINE_MEMBER( at_mb_device::dma8237_2_out_eop )
{
	m_cur_eop2 = state == ASSERT_LINE;
	if(m_dma_channel != -1)
		m_isabus->eop_w(m_dma_channel, m_cur_eop2 ? ASSERT_LINE : CLEAR_LINE );
}

void at_mb_device::set_dma_channel(int channel, int state)
{
	if(!state) {
		m_dma_channel = channel;
		if(m_cur_eop)
			m_isabus->eop_w(channel, ASSERT_LINE );

	} else if(m_dma_channel == channel) {
		m_dma_channel = -1;
		if(m_cur_eop)
			m_isabus->eop_w(channel, CLEAR_LINE );
	}
}

WRITE8_MEMBER( at_mb_device::write_rtc )
{
	if (offset==0) {
		m_nmi_enabled = BIT(data,7);
		m_isabus->set_nmi_state((m_nmi_enabled==0) && (m_channel_check==0));
		m_mc146818->write(space,0,data);
	}
	else {
		m_mc146818->write(space,offset,data);
	}
}

uint32_t at_mb_device::a20_286(bool state)
{
	return (state ? 0xffffff : 0xefffff);
}

WRITE_LINE_MEMBER( at_mb_device::shutdown )
{
	if(state)
		m_maincpu->reset();
}
WRITE_LINE_MEMBER( at_mb_device::dack0_w ) { set_dma_channel(0, state); }
WRITE_LINE_MEMBER( at_mb_device::dack1_w ) { set_dma_channel(1, state); }
WRITE_LINE_MEMBER( at_mb_device::dack2_w ) { set_dma_channel(2, state); }
WRITE_LINE_MEMBER( at_mb_device::dack3_w ) { set_dma_channel(3, state); }
WRITE_LINE_MEMBER( at_mb_device::dack4_w ) { m_dma8237_1->hack_w(state ? 0 : 1); } // it's inverted
WRITE_LINE_MEMBER( at_mb_device::dack5_w ) { set_dma_channel(5, state); }
WRITE_LINE_MEMBER( at_mb_device::dack6_w ) { set_dma_channel(6, state); }
WRITE_LINE_MEMBER( at_mb_device::dack7_w ) { set_dma_channel(7, state); }

READ8_MEMBER( at_mb_device::portb_r )
{
	uint8_t data = m_at_speaker;
	data &= ~0xd0; /* AT BIOS don't likes this being set */

	/* 0x10 is the dram refresh line bit, 15.085us. */
	data |= (machine().time().as_ticks(110000) & 1) ? 0x10 : 0;

	if (m_pit_out2)
		data |= 0x20;
	else
		data &= ~0x20; /* ps2m30 wants this */

	return data;
}

WRITE8_MEMBER( at_mb_device::portb_w )
{
	m_at_speaker = data;
	m_pit8254->write_gate2(BIT(data, 0));
	speaker_set_spkrdata( BIT(data, 1));
	m_channel_check = BIT(data, 3);
	m_isabus->set_nmi_state((m_nmi_enabled==0) && (m_channel_check==0));
}
