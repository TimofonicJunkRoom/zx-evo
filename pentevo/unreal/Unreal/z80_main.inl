
// ����� ����� ��������� 0xFFFF
// (����� � ������ ������� ������ � ���������� �� ������ &= 0xFFFF)
u8 rm(unsigned addr)
{
   addr &= 0xFFFF;

#ifdef Z80_DBG
   u8 *membit = membits + (addr & 0xFFFF);
   *membit |= MEMBITS_R;
   dbgbreak |= (*membit & MEMBITS_BPR);
   cpu.dbgbreak |= (*membit & MEMBITS_BPR);
#endif

#ifdef MOD_GSZ80
   if ((temp.gsdmaon!=0) && ( (conf.mem_model==MM_PENTAGON) || (conf.mem_model==MM_ATM3) ) && ((addr & 0xc000)==0) && ((comp.pEFF7 & EFF7_ROCACHE)==0))
    {
     u8 *tmp;
     tmp = GSRAM_M+((temp.gsdmaaddr-1) & 0x1FFFFF);
     temp.gsdmaaddr = (temp.gsdmaaddr + 1) & 0x1FFFFF;
     z80gs::flush_gs_z80();
     return *tmp;
    }
#endif

	// TS-conf DMA
	if (conf.mem_model == MM_TSL && comp.ts.dma.act && cpu.t >= comp.ts.dma.next_t) dma();

	// TS-conf cache model
	u8 window = (addr >> 14) & 3;
	if (bankm[window])		// RAM hit
	{
		if (conf.mem_model == MM_TSL)
		{
			u32 cached_address = (comp.ts.page[window] << 5) | ((addr >> 9) & 0x1F);
			u16 cache_pointer = addr & 0x1FF;
			if (!comp.ts.cache || (cpu.tscache[cache_pointer] != cached_address))
				vid.memcpucyc[cpu.t / 224]++;		// ��� ���������
			cpu.tscache[cache_pointer] = cached_address;
		}
		else
			vid.memcpucyc[cpu.t / 224]++;
	}

   return *am_r(addr);
}

// ����� ����� ��������� 0xFFFF
// (����� � ������ ������� ������ � ���������� �� ������ &= 0xFFFF)
void wm(unsigned addr, u8 val)
{
   addr &= 0xFFFF;

#ifdef Z80_DBG
   u8 *membit = membits + (addr & 0xFFFF);
   *membit |= MEMBITS_W;
   dbgbreak |= (*membit & MEMBITS_BPW);
   cpu.dbgbreak |= (*membit & MEMBITS_BPW);
#endif

#ifdef MOD_GSZ80
   if ((temp.gsdmaon!=0) && ( (conf.mem_model==MM_PENTAGON) || (conf.mem_model==MM_ATM3) ) && ((addr & 0xc000)==0))
    {
     u8 *tmp;
     tmp = GSRAM_M+temp.gsdmaaddr;
     *tmp = val;
     temp.gsdmaaddr++;
     z80gs::flush_gs_z80();
    }
#endif

#ifdef MOD_VID_VD
   if (comp.vdbase && (unsigned)((addr & 0xFFFF) - 0x4000) < 0x1800)
   {
       comp.vdbase[addr & 0x1FFF] = val;
       return;
   }
#endif

   // TS palette load
   if ((conf.mem_model == MM_TSL) && (comp.ts.fm_en))
		if (((addr >> 12) & 0x0F) == comp.ts.fm_addr)
			if (addr & 1)
			// write to FPGA RAM
			switch ((addr >> 9) & 0x07)
			{
				case TSF_CRAM:
				{
					update_screen();
					comp.cram[(addr >> 1) & 0xFF] = ((val << 8) | temp.fm_tmp) & 0x7FFF;		// 15 bits of CRAM data
					update_clut((addr >> 1) & 0xFF);
					break;
				}
				case TSF_SFILE:
				{
					update_screen();
					comp.sfile[(addr >> 1) & 0xFF] = (val << 8) | temp.fm_tmp;
					break;
				}
			}

			else
			// remember temp value
				temp.fm_tmp = val;

	// TS-conf DMA
	if (conf.mem_model == MM_TSL && comp.ts.dma.act && cpu.t >= comp.ts.dma.next_t) dma();

	// TS-conf cache model
	if (conf.mem_model == MM_TSL)
	{
		u16 cache_pointer = addr & 0x1FF;
		cpu.tscache[cache_pointer] = -1;
		vid.memcpucyc[cpu.t / 224]++;
	}

   if ((conf.mem_model == MM_ATM3) && (comp.pBF & 4) /*&& ((addr & 0xF800) == 0)*/ ) // ��������� �������� ������ ��� ATM3 // lvd: any addr is possible in ZXEVO
   {
       update_screen();
       unsigned idx = ((addr&0x07F8) >> 3) | ((addr & 7) << 8);
       fontatm2[idx] = val;
       return;
   }

   u8 *a = bankw[(addr >> 14) & 3];
#ifndef TRASH_PAGE
   if (!a)
       return;
#endif
   a += (addr & (PAGE-1));

   if (*a == val)
     return;

   update_screen();
   *a = val;
}

Z80INLINE u8 m1_cycle(Z80 *cpu)
{
   if ((conf.mem_model == MM_PENTAGON) &&
       ((comp.pEFF7 & (EFF7_CMOS | EFF7_4BPP)) == (EFF7_CMOS | EFF7_4BPP)))
       temp.offset_vscroll++;
   if ((conf.mem_model == MM_PENTAGON) &&
      ((comp.pEFF7 & (EFF7_384 | EFF7_4BPP)) == (EFF7_384 | EFF7_4BPP)))
       temp.offset_hscroll++;
   if (conf.mem_model == MM_TSL && comp.ts.vdos_m1) {
      comp.ts.vdos    = 1;
      comp.ts.vdos_m1 = 0;
      set_banks();
   }
   cpu->r_low++;// = (cpu->r & 0x80) + ((cpu->r+1) & 0x7F);
   cputact(4);
   return rm(cpu->pc++);
}

void Z80FAST step()
{
   if (comp.flags & CF_SETDOSROM)
   {
      if (cpu.pch == 0x3D)
      {
           comp.flags |= CF_TRDOS;	// !!! add here TS memconf behaviour !!!
           set_banks();
      }
   }
   else if (comp.flags & CF_LEAVEDOSADR)
   {
      if (cpu.pch & 0xC0) // PC > 3FFF closes TR-DOS
      {
         close_dos: comp.flags &= ~CF_TRDOS;
         set_banks();
      }
      if (conf.trdos_traps)
          comp.wd.trdos_traps();
   }
   else if (comp.flags & CF_LEAVEDOSRAM)
   {
      // executing RAM closes TR-DOS
      if (bankr[(cpu.pc >> 14) & 3] < RAM_BASE_M+PAGE*MAX_RAM_PAGES)
          goto close_dos;
      if (conf.trdos_traps)
          comp.wd.trdos_traps();
   }

   if (comp.tape.play_pointer && conf.tape_traps && (cpu.pc & 0xFFFF) == 0x056B)
       tape_traps();

   if (comp.tape.play_pointer && !conf.sound.enabled)
       fast_tape();

//todo if (comp.turbo)cpu.t-=tbias[cpu.dt]
   if (cpu.pch & temp.evenM1_C0)
       cpu.t += (cpu.t & 1);
//~todo
//[vv]   unsigned oldt=cpu.t; //0.37
   u8 opcode = m1_cycle(&cpu);
   (normal_opcode[opcode])(&cpu);

/* [vv]
//todo if (comp.turbo)cpu.t-=tbias[cpu.t-oldt]
   if ( ((conf.mem_model == MM_PENTAGON) && ((comp.pEFF7 & EFF7_GIGASCREEN)==0)) ||
       ((conf.mem_model == MM_ATM710) && (comp.pFF77 & 8)))
       cpu.t -= (cpu.t-oldt) >> 1; //0.37
//~todo
*/
#ifdef Z80_DBG
   if ((comp.flags & CF_PROFROM) && ((membits[0x100] | membits[0x104] | membits[0x108] | membits[0x10C]) & MEMBITS_R))
   {
      if (membits[0x100] & MEMBITS_R)
          set_scorp_profrom(0);
      if (membits[0x104] & MEMBITS_R)
          set_scorp_profrom(1);
      if (membits[0x108] & MEMBITS_R)
          set_scorp_profrom(2);
      if (membits[0x10C] & MEMBITS_R)
          set_scorp_profrom(3);
   }
#endif
}

void step1()
{
#ifdef Z80_DBG
	debug_events(&cpu);
#endif
	step();
}
/*
void try_int()
{
	if (!cpu.iff1 ||	// int NOT enabled in CPU
		((conf.mem_model == MM_ATM710) && !(comp.pFF77 & 0x20)) || // int enabled by ATM hardware -- lvd added no int disabling in pentevo (atm3)
    ((conf.mem_model == MM_TSL) && (comp.ts.vdos || comp.ts.vdos_m1))) // int disabled in vdos after r/w vg ports
		return;
		
	else	// int enabled
	{
		if (cpu.t == cpu.eipos)		// if INT issued right after EI, it's delayed for 1 opcode
			step1();
		handle_int(&cpu, cpu.IntVec()); // ������ ��������� int (������ � ���� ������ �������� � �.�.)
	}
}
*/

void z80loop_TSL()
{
  cpu.haltpos = 0;
  comp.ts.intctrl.new_frame = true;
  comp.ts.intctrl.line_t = comp.ts.intline ? 0 : VID_TACTS;

  while (cpu.t < conf.frame)
  {
    bool vdos = comp.ts.vdos || comp.ts.vdos_m1;

    // Frame INT
    if (comp.ts.intctrl.new_frame && ((cpu.t - comp.ts.intctrl.frame_t) < conf.intlen))
    {
      comp.ts.intctrl.new_frame = false;
      if (!vdos) comp.ts.intctrl.frame_pend = comp.ts.intframe;
    }
    else if (comp.ts.intctrl.frame_pend && ((cpu.t - comp.ts.intctrl.frame_t) >= conf.intlen))
      comp.ts.intctrl.frame_pend = 0;

    // Line INT
    if (cpu.t >= comp.ts.intctrl.line_t)
    {
      comp.ts.intctrl.line_t += VID_TACTS;
      if (!vdos) comp.ts.intctrl.line_pend = comp.ts.intline;
    }

    // DMA INT
    if (comp.ts.intctrl.new_dma)
    {
      comp.ts.intctrl.new_dma = false;
      comp.ts.intctrl.dma_pend = comp.ts.intdma;
    }

    if (comp.ts.intctrl.pend && cpu.iff1 && cpu.t != cpu.eipos && !vdos) // int disabled in vdos after r/w vg ports
    {
      handle_int(&cpu, cpu.IntVec());
    }

    step1();
  }
}

void z80loop_other()
{
  cpu.haltpos = 0;
  cpu.int_pend = true;

  while (cpu.t < conf.frame)
  {
    // Baseconf NMI trap
    if (conf.mem_model == MM_ATM3 && (comp.pBF & 0x10) && (cpu.pc == comp.pBD))
      nmi_pending = 1;

    // NMI processing
    if (nmi_pending)
    {
      if (conf.mem_model == MM_ATM3)
      {
        nmi_pending = 0;
        cpu.nmi_in_progress = true;
        set_banks();
        m_nmi(RM_NOCHANGE);
        continue;
      }
      else if (conf.mem_model == MM_PROFSCORP || conf.mem_model == MM_SCORP)
      {
        nmi_pending--;
        if (cpu.pc > 0x4000)
        {
          m_nmi(RM_DOS);
          nmi_pending = 0;
        }
      }
      else
        nmi_pending = 0;
    } // end if (nmi_pending)

    // Baseconf NMI
    if (comp.pBE)
    {
      if (conf.mem_model == MM_ATM3 && comp.pBE == 1)
      {
        cpu.nmi_in_progress = false;
        set_banks();
      }
      comp.pBE--;
    }

    // Reset INT
    if (cpu.int_pend && (cpu.t >= conf.intlen))
      cpu.int_pend = false;

    // INT
    if (cpu.int_pend && cpu.iff1 && // INT enabled in CPU
      cpu.t != cpu.eipos &&         // INT disabled after EI
      cpu.int_gate)                 // INT enabled by ATM hardware (no INT disabling in PentEvo)
    {
      handle_int(&cpu, cpu.IntVec());
    }

    step1();
  } // end while (cpu.t < conf.intlen)
}


void z80loop()
{
  if (conf.mem_model == MM_TSL)
    z80loop_TSL();
  else
    z80loop_other();
}

/*
void z80loop()
{
   cpu.haltpos = 0;
   cpu.intnew = true;
   comp.ts.intctrl.new_frame = 1;
   comp.ts.intctrl.line_t = comp.ts.intline ? 0 : VID_TACTS;
   
	// main loop
	while (cpu.t < conf.frame)
	{
		// Baseconf NMI breakpoints
		if (comp.pBF & 0x10)	// ! add here MM_ATM3 check !
			if (cpu.pc == comp.pBD)
				nmi_pending = 1;

		// NMI processing
		if (nmi_pending)
		{
			if (conf.mem_model == MM_ATM3)
			{
				nmi_pending = 0;
				cpu.nmi_in_progress = true;
				set_banks();
				m_nmi(RM_NOCHANGE);
			}
			else if ((conf.mem_model == MM_PROFSCORP || conf.mem_model == MM_SCORP))
			{
				nmi_pending = 0;
				if (cpu.pc >= 0x4000)
				{
					// printf("pc=%x\n", cpu.pc);
					::m_nmi(RM_DOS);
					nmi_pending = 0;
				}
			}
			else
				nmi_pending = 0;
		}

    if (conf.mem_model == MM_TSL)
    {
      // Generate frame INT
      if (cpu.intnew && ((cpu.t - comp.ts.intctrl.frame_t) < conf.intlen))
      {
        cpu.intnew = false;
        comp.ts.intctrl.frame_pend = comp.ts.intframe;
      }
      else if (comp.ts.intctrl.frame_pend && ((cpu.t - comp.ts.intctrl.frame_t + 1) >= conf.intlen))
        comp.ts.intctrl.frame_pend = 0;

      // Generate line INT
      if (comp.ts.intline && cpu.t >= comp.ts.intctrl.line_t)
      {
        comp.ts.intctrl.line_pend = comp.ts.intline;
        comp.ts.intctrl.line_t += VID_TACTS;
      }

      if (comp.ts.intctrl.pend)
        try_int();

      step1();
    }
    else if (cpu.intnew && (cpu.t < conf.intlen))
		{ // INT loop
			cpu.int_pend = true;
			cpu.intnew = false;
			while (cpu.int_pend && (cpu.t < conf.intlen))
			{
				try_int();
				step1();
			}
			
			cpu.int_pend = false;
			cpu.eipos = -1;
		}
		else
		step1();

		// Baseconf NMI
		if (comp.pBE)
		{
			if (comp.pBE == 1 && conf.mem_model == MM_ATM3)
			{
				cpu.nmi_in_progress = false;
				set_banks();
			}
			comp.pBE--;
		}


		// if (cpu.halted)
			// break;
		
//
//     if (cpu.halted)
//    {
//        //cpu.t += 4, cpu.r = (cpu.r & 0x80) + ((cpu.r+1) & 0x7F); continue;
//        unsigned st = (conf.frame-cpu.t-1)/4+1;
//        cpu.t += 4*st;
//        cpu.r_low += st;
//        break;
//    }
//  
	}
}
*/