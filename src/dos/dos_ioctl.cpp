/*
 *  Copyright (C) 2002-2005  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: dos_ioctl.cpp,v 1.24 2005-04-11 17:56:27 qbix79 Exp $ */

#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_inc.h"

bool DOS_IOCTL(void) {
	Bitu handle=0;Bit8u drive=0;
	if (reg_al<8) {				/* call 0-7 use a file handle */
		handle=RealHandle(reg_bx);
		if (handle>=DOS_FILES) {
			DOS_SetError(DOSERR_INVALID_HANDLE);
			return false;
		}
		if (!Files[handle]) {
			DOS_SetError(DOSERR_INVALID_HANDLE);
			return false;
		}
	} else  { 				/* the others use a diskdrive */
		drive=reg_bl;if (!drive) drive=dos.current_drive;else drive--;
		if( !(( drive < DOS_DRIVES ) && Drives[drive]) ) {
			DOS_SetError(DOSERR_INVALID_DRIVE);
			return false;
		}
	}
	switch(reg_al) {
	case 0x00:		/* Get Device Information */
		reg_dx=Files[handle]->GetInformation();
		reg_ax=reg_dx; //Destroyed officially
		return true;
	case 0x06:      /* Get Input Status */
		if (Files[handle]->GetInformation() & 0x8000) {		//Check for device
			reg_al=(Files[handle]->GetInformation() & 0x40) ? 0x0 : 0xff;
		} else { // FILE
			Bit32u oldlocation=0;
			Files[handle]->Seek(&oldlocation, DOS_SEEK_CUR);
			Bit32u endlocation=0;
			Files[handle]->Seek(&endlocation, DOS_SEEK_END);
			if(oldlocation < endlocation){//Still data available
				reg_al=0xff;
			} else
			{
				reg_al=0x0; //EOF or beyond
			}
			Files[handle]->Seek(&oldlocation, DOS_SEEK_SET); //restore filelocation
			LOG(LOG_IOCTL,LOG_NORMAL)("06:Used Get Input Status on regualar file with handle %d",handle);
		}
		return true;
	case 0x07:		/* Get Output Status */
		LOG(LOG_IOCTL,LOG_NORMAL)("07:Fakes output status is ready for handle %d",handle);
		reg_al=0xff;
		return true;
	case 0x08:		/* Check if block device removable */
		/* cdrom drives and drive a&b are removable */
		if (drive < 2  || Drives[drive]->isRemovable())
			reg_ax=0;
		else 	reg_ax=1;
		return true;
	case 0x09:		/* Check if block device remote */
		reg_dx=0;
		if (Drives[drive]->isRemote()) reg_dx|=(1 << 12);
		//TODO Set bit 9 on drives that don't support direct I/O
		reg_al=0;
		return true;
	case 0x0D:		/* Generic block device request */
		{
			PhysPt ptr	= SegPhys(ds)+reg_dx;
			switch (reg_cl) {
			case 0x60:		/* Get Device parameters */
				mem_writeb(ptr  ,0x03);					// special function
				mem_writeb(ptr+1,(drive>=2)?0x05:0x14);	// fixed disc(5), 1.44 floppy(14)
				mem_writew(ptr+2,drive>=2);				// nonremovable ?
				mem_writew(ptr+4,0x0000);				// num of cylinders
				mem_writeb(ptr+6,0x00);					// media type (00=other type)
				// drive parameter block following
				mem_writeb(ptr+7,drive);				// drive
				mem_writeb(ptr+8,0x00);					// unit number
				mem_writed(ptr+0x1f,0xffffffff);		// next parameter block
				break;
			case 0x46:
			case 0x66:	/* Volume label */
				{			
					char const* bufin=Drives[drive]->GetLabel();
					char buffer[11] ={' '};

					char* find_ext=strchr(bufin,'.');
					if (find_ext) {
						Bitu size=find_ext-bufin;if (size>8) size=8;
						memcpy(buffer,bufin,size);
						find_ext++;
						memcpy(buffer+size,find_ext,(strlen(find_ext)>3) ? 3 : strlen(find_ext)); 
					} else {
						memcpy(buffer,bufin,(strlen(bufin) > 8) ? 8 : strlen(bufin));
					}
			
					char buf2[8]={ 'F','A','T','1','6',' ',' ',' '};
					if(drive<2) buf2[4] = '2'; //FAT12 for floppies

					mem_writew(ptr+0,0);			// 0
					mem_writed(ptr+2,0x1234);		//Serial number
					MEM_BlockWrite(ptr+6,buffer,11);//volumename
					if(reg_cl == 0x66) MEM_BlockWrite(ptr+0x11, buf2,8);//filesystem
				}
				break;
			default	:	
				LOG(LOG_IOCTL,LOG_ERROR)("DOS:IOCTL Call 0D:%2X Drive %2X unhandled",reg_cl,drive);
				return false;
			}
			return true;
		}
	case 0xE:		/* Get Logical Drive Map */
		reg_al = 0;		/* Only 1 logical drive assigned */
		return true;
	default:
		LOG(LOG_DOSMISC,LOG_ERROR)("DOS:IOCTL Call %2X unhandled",reg_al);
		return false;
	};
	return false;
};


bool DOS_GetSTDINStatus(void) {
	Bit32u handle=RealHandle(STDIN);
	if (handle==0xFF) return false;
	if (Files[handle] && (Files[handle]->GetInformation() & 64)) return false;
	return true;
};
