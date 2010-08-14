/*
     This file is part of libextractor.
     (C) 2004, 2009 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
 */

#include "platform.h"
#include "extractor.h"
#include "pack.h"
#include <stdint.h>

typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;

typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;

/* first 4 bytes of the ELF header */
static char elfMagic[] = { 0x7f, 'E', 'L', 'F' };

#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_NIDENT 16

typedef struct
{
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;            /* offset of the section header table */
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phensize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;       /* size of each entry in SH table */
  Elf32_Half e_shnum;           /* how many entries in section header table */
  Elf32_Half e_shstrndx;        /* section header's sh_name member is index into this string table! */
} Elf32_Ehdr;

/* elf-header minus e_ident */
#define ELF_HEADER_SIZE sizeof (Elf32_Ehdr)

#define ELF_HEADER_FIELDS(p) \
  &(p)->e_type,		     \
    &(p)->e_machine,	     \
    &(p)->e_version,	     \
    &(p)->e_entry,	     \
    &(p)->e_phoff,	     \
    &(p)->e_shoff,	     \
    &(p)->e_flags,	     \
    &(p)->e_ehsize,	     \
    &(p)->e_phensize,	     \
    &(p)->e_phnum,	     \
    &(p)->e_shentsize,	     \
    &(p)->e_shnum,	     \
    &(p)->e_shstrndx
static char *ELF_HEADER_SPECS[] = {
  "hhwwwwwhhhhhh",
  "HHWWWWWHHHHHH",
};

typedef struct {
        Elf64_Half      e_type;
        Elf64_Half      e_machine;
        Elf64_Word      e_version;
        Elf64_Addr      e_entry;
        Elf64_Off       e_phoff;
        Elf64_Off       e_shoff;
        Elf64_Word      e_flags;
        Elf64_Half      e_ehsize;
        Elf64_Half      e_phensize;
        Elf64_Half      e_phnum;
        Elf64_Half      e_shentsize;
        Elf64_Half      e_shnum;
        Elf64_Half      e_shstrndx;
} Elf64_Ehdr;

/* elf-header minus e_ident */
#define ELF64_HEADER_SIZE sizeof (Elf64_Ehdr)

#define ELF64_HEADER_FIELDS(p) \
    &(p)->e_type,		     \
    &(p)->e_machine,	     \
    &(p)->e_version,	     \
    &(p)->e_entry,	     \
    &(p)->e_phoff,	     \
    &(p)->e_shoff,	     \
    &(p)->e_flags,	     \
    &(p)->e_ehsize,	     \
    &(p)->e_phensize,	     \
    &(p)->e_phnum,	     \
    &(p)->e_shentsize,	     \
    &(p)->e_shnum,	     \
    &(p)->e_shstrndx
static char *ELF64_HEADER_SPECS[] = {
  "hhwxxxwhhhhhh",
  "HHWXXXWHHHHHH",
};


typedef struct
{
  Elf32_Word sh_name;
  Elf32_Word sh_type;
  Elf32_Word sh_flags;
  Elf32_Addr sh_addr;           /* where loaded */
  Elf32_Off sh_offset;          /* where in image (! sh_type==SHT_NOBITS) */
  Elf32_Word sh_size;           /* section size in bytes */
  Elf32_Word sh_link;           /* for symbol table: section header index of the associated string table! */
  Elf32_Word sh_info;           /* "one greater than the symbol table index of the last local symbol _STB_LOCAL_" */
  Elf32_Word sh_addralign;
  Elf32_Word sh_entsize;
} Elf32_Shdr;
#define ELF_SECTION_SIZE 40

#define ELF_SECTION_FIELDS(p) \
  &(p)->sh_name,	      \
    &(p)->sh_type,	      \
    &(p)->sh_flags,	      \
    &(p)->sh_addr,	      \
    &(p)->sh_offset,	      \
    &(p)->sh_size,	      \
    &(p)->sh_link,	      \
    &(p)->sh_info,	      \
    &(p)->sh_addralign,	      \
    &(p)->sh_entsize
static char *ELF_SECTION_SPECS[] = {
  "wwwwwwwwww",
  "WWWWWWWWWW",
};

typedef struct
{
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
} Elf32_Phdr;
#define ELF_PDHR_SIZE 32
#define ELF_PHDR_FIELDS(p)	   \
  &(p)->p_type,			   \
    &(p)->p_offset,		   \
    &(p)->p_vaddr,		   \
    &(p)->p_paddr,		   \
    &(p)->p_filesz,		   \
    &(p)->p_memsz,		   \
    &(p)->p_flags,		   \
    &(p)->p_align
static char *ELF_PHDR_SPECS[] = {
  "wwwwwwww",
  "WWWWWWWW",
};

typedef struct
{
  Elf32_Sword d_tag;
  union
  {
    Elf32_Word d_val;
    Elf32_Addr d_ptr;
  } d_un;
} Elf32_Dyn;
#define ELF_DYN_SIZE 8
#define ELF_DYN_FIELDS(p)			\
  &(p)->d_tag,					\
    &(p)->d_un
static char *ELF_DYN_SPECS[] = {
  "ww",
  "WW",
};

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4
#define ET_LOPROC 0xff00
#define ET_HIPROC 0xffff

#define EM_NONE 0
#define EM_M32 1
#define EM_SPARC 2
#define EM_386 3
#define EM_68K 4
#define EM_88K 5
#define EM_860 7
#define EM_MIPS 8
#define EM_PPC 20
#define EM_PPC64 21
#define EM_S390 22
#define EM_ARM 40
#define EM_ALPHA 41
#define EM_IA_64 50
#define EM_X86_64 62
#define EM_CUDA 190

#define ELFOSABI_NETBSD 2
#define ELFOSABI_LINUX 3
#define ELFOSABI_IRIX 8
#define ELFOSABI_FREEBSD 9
#define ELFOSABI_OPENBSD 12

#define EV_NONE 0
#define EV_CURRENT 1

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
/* string table! */
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
/* dynamic linking info! */
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9
#define SHT_SHLIB 10
#define SHT_DYNSYM 11
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7fffffff
#define SHT_LOUSER 0x80000000
#define SHT_HIUSER 0xffffffff

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4
#define SHF_MASKPROC 0xf000000

#define DT_NULL 0
/* name of a needed library, offset into table
   recorded in DT_STRTAB entry */
#define DT_NEEDED 1
#define DT_PLTRELSZ 2
#define DT_PLTGOT 3
#define DT_HASH 4
/* address of the string table from where symbol
   names, library names, etc for this DT come from */
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_SYMENT 7
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
/* size of the string-table in bytes */
#define DT_STRSZ 10
/* fixme 11 */
#define DT_INIT 12
#define DT_FINI 13
/* string-table offset giving the name of the shared object */
#define DT_SONAME 14
/* string-table offset of a null-terminated library search path */
#define DT_RPATH 15
#define DT_SYMBOLIC 16


#define PT_NULL 0
#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PT_INTERP 3
#define PT_NOTE 4
#define PT_SHLIB 5
#define PT_PHDR 6
#define PT_LOPROC 0x70000000
#define PT_HIPROC 0x7fffffff




#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2

#define ELFDATANONE 0
/* little endian */
#define ELFDATA2LSB 1
/* big endian */
#define ELFDATA2MSB 2

/**
 * @param ei_data ELFDATA2LSB or ELFDATA2MSB
 * @return 1 if we need to convert, 0 if not
 */
static int
getByteorder (char ei_data)
{
  if (ei_data == ELFDATA2LSB)
    {
#if __BYTE_ORDER == __BIG_ENDIAN
      return 1;
#else
      return 0;
#endif
    }
  else
    {
#if __BYTE_ORDER == __BIG_ENDIAN
      return 0;
#else
      return 1;
#endif
    }
}

/**
 *
 * @return 0 on success, -1 on error
 */
static int
getSectionHdr (const char *data,
               size_t size,
               Elf32_Ehdr * ehdr, Elf32_Half idx, Elf32_Shdr * ret)
{
  if (ehdr->e_shnum <= idx)
    return -1;

  EXTRACTOR_common_cat_unpack (&data[ehdr->e_shoff + ehdr->e_shentsize * idx],
              ELF_SECTION_SPECS[getByteorder (data[EI_CLASS])],
              ELF_SECTION_FIELDS (ret));
  return 0;
}

/**
 *
 * @return 0 on success, -1 on error
 */
static int
getDynTag (const char *data,
           size_t size,
           Elf32_Ehdr * ehdr,
           Elf32_Off off, Elf32_Word osize, unsigned int idx, Elf32_Dyn * ret)
{
  if ((off + osize > size) || ((idx + 1) * ELF_DYN_SIZE > osize))
    return -1;
  EXTRACTOR_common_cat_unpack (&data[off + idx * ELF_DYN_SIZE],
			       ELF_DYN_SPECS[getByteorder (data[EI_CLASS])],
			       ELF_DYN_FIELDS (ret));
  return 0;
}

/**
 *
 * @return 0 on success, -1 on error
 */
static int
getProgramHdr (const char *data,
               size_t size,
               Elf32_Ehdr * ehdr, Elf32_Half idx, Elf32_Phdr * ret)
{
  if (ehdr->e_phnum <= idx)
    return -1;

  EXTRACTOR_common_cat_unpack (&data[ehdr->e_phoff + ehdr->e_phensize * idx],
              ELF_PHDR_SPECS[getByteorder (data[EI_CLASS])],
              ELF_PHDR_FIELDS (ret));
  return 0;
}

/**
 * Parse ELF header.
 * @return 0 on success for 32 bit, 1 on success for 64 bit, -1 on error
 */
static int
getELFHdr (const char *data, 
	   size_t size,
	   Elf32_Ehdr * ehdr,
	   Elf64_Ehdr * ehdr64)
{
  /* catlib */
  if (size < EI_NIDENT)
    return -1;
  if (0 != strncmp (data, elfMagic, sizeof (elfMagic)))
    return -1;                  /* not an elf */

  switch (data[EI_CLASS])
    {
    case ELFCLASS32:
      if (size < sizeof (Elf32_Ehdr) + EI_NIDENT)
	return -1;
      EXTRACTOR_common_cat_unpack (&data[EI_NIDENT],
				   ELF_HEADER_SPECS[getByteorder (data[EI_DATA])],
				   ELF_HEADER_FIELDS (ehdr));
      if (ehdr->e_shoff + ehdr->e_shentsize * ehdr->e_shnum > size)
	return -1;                  /* invalid offsets... */
      if (ehdr->e_shentsize < ELF_SECTION_SIZE)
	return -1;                  /* huh? */
      if (ehdr->e_phoff + ehdr->e_phensize * ehdr->e_phnum > size)
	return -1;
      return 0;
    case ELFCLASS64:
      if (size < sizeof (Elf64_Ehdr) + EI_NIDENT)
	return -1;
      EXTRACTOR_common_cat_unpack (&data[EI_NIDENT],
				   ELF64_HEADER_SPECS[getByteorder (data[EI_DATA])],
				   ELF64_HEADER_FIELDS (ehdr64));
      if (ehdr64->e_shoff + ((uint32_t) ehdr64->e_shentsize * ehdr64->e_shnum) > size)
	return -1;                  /* invalid offsets... */
      if (ehdr64->e_phoff + ((uint32_t) ehdr64->e_phensize * ehdr64->e_phnum) > size)
	return -1;
      return 1;
    default:
      return -1;
    }
}

/**
 * @return the string (offset into data, do NOT free), NULL on error
 */
static const char *
readStringTable (const char *data,
                 size_t size,
                 Elf32_Ehdr * ehdr,
                 Elf32_Half strTableOffset, Elf32_Word sh_name)
{
  Elf32_Shdr shrd;
  if (-1 == getSectionHdr (data, size, ehdr, strTableOffset, &shrd))
    return NULL;
  if ((shrd.sh_type != SHT_STRTAB) ||
      (shrd.sh_offset + shrd.sh_size > size) ||
      (shrd.sh_size <= sh_name) ||
      (data[shrd.sh_offset + shrd.sh_size - 1] != '\0'))
    return NULL;
  return &data[shrd.sh_offset + sh_name];
}

#define ADD(s, type) do { if (0!=proc(proc_cls, "elf", type, EXTRACTOR_METAFORMAT_UTF8, "text/plain", s, strlen(s)+1)) return 1; } while (0)

/* application/x-executable, ELF */
int 
EXTRACTOR_elf_extract (const char *data,
		       size_t size,
		       EXTRACTOR_MetaDataProcessor proc,
		       void *proc_cls,
		       const char *options)
{
  Elf32_Ehdr ehdr;
  Elf32_Half idx;
  Elf64_Ehdr ehdr64;
  int ret;

  ret = getELFHdr (data, size, &ehdr, &ehdr64);
  if (ret == -1)
    return 0;
  ADD ("application/x-executable", EXTRACTOR_METATYPE_MIMETYPE);
  switch ( ((unsigned char*) data)[EI_OSABI])
    {
    case ELFOSABI_LINUX:
      ADD ("Linux", EXTRACTOR_METATYPE_TARGET_OS);
      break;
    case ELFOSABI_FREEBSD:
      ADD ("FreeBSD", EXTRACTOR_METATYPE_TARGET_OS);
      break;
    case ELFOSABI_NETBSD:
      ADD ("NetBSD", EXTRACTOR_METATYPE_TARGET_OS);
      break;
    case ELFOSABI_OPENBSD:
      ADD ("OpenBSD", EXTRACTOR_METATYPE_TARGET_OS);
      break;
    case ELFOSABI_IRIX:
      ADD ("IRIX", EXTRACTOR_METATYPE_TARGET_OS);
      break;
    default:
      break;
    }
  switch ( (ret == 0) ? ehdr.e_type : ehdr64.e_type) 
    {
    case ET_REL:
      ADD ("Relocatable file", EXTRACTOR_METATYPE_RESOURCE_TYPE);
      break;
    case ET_EXEC:
      ADD ("Executable file", EXTRACTOR_METATYPE_RESOURCE_TYPE);
      break;
    case ET_DYN:
      ADD ("Shared object file", EXTRACTOR_METATYPE_RESOURCE_TYPE);
      break;
    case ET_CORE:
      ADD ("Core file", EXTRACTOR_METATYPE_RESOURCE_TYPE);
      break;
    default:
      break;                    /* unknown */
    }
  switch ( (ret == 0) ? ehdr.e_machine : ehdr64.e_machine)
    {
    case EM_M32:
      ADD ("M32", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_386:
      ADD ("i386", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_68K:
      ADD ("68K", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_88K:
      ADD ("88K", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_SPARC:
      ADD ("Sparc", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_860:
      ADD ("960", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_MIPS:
      ADD ("MIPS", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_PPC:
      ADD ("PPC", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_PPC64:
      ADD ("PPC64", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_S390:
      ADD ("S390", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_ARM:
      ADD ("ARM", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_ALPHA:
      ADD ("ALPHA", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_IA_64:
      ADD ("IA-64", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_X86_64:
      ADD ("x86_64", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    case EM_CUDA:
      ADD ("NVIDIA CUDA", EXTRACTOR_METATYPE_TARGET_ARCHITECTURE);
      break;
    default:
      break;                    /* oops */
    }

  if (ret != 0)
    return 0; /* FIXME: full support for 64-bit ELF... */
  for (idx = 0; idx < ehdr.e_phnum; idx++)
    {
      Elf32_Phdr phdr;

      if (0 != getProgramHdr (data, size, &ehdr, idx, &phdr))
        return 0;
      if (phdr.p_type == PT_DYNAMIC)
        {
          unsigned int dc = phdr.p_filesz / ELF_DYN_SIZE;
          unsigned int id;
          Elf32_Addr stringPtr;
          Elf32_Half stringIdx;
          Elf32_Half six;

          stringPtr = 0;

          for (id = 0; id < dc; id++)
            {
              Elf32_Dyn dyn;
              if (0 != getDynTag (data,
                                  size,
                                  &ehdr,
                                  phdr.p_offset, phdr.p_filesz, id, &dyn))
                return 0;
              if (DT_STRTAB == dyn.d_tag)
                {
                  stringPtr = dyn.d_un.d_ptr;
                  break;
                }
            }
          if (stringPtr == 0)
            return 0;
          for (six = 0; six < ehdr.e_shnum; six++)
            {
              Elf32_Shdr sec;
              if (-1 == getSectionHdr (data, size, &ehdr, six, &sec))
                return 0;
              if ((sec.sh_addr == stringPtr) && (sec.sh_type == SHT_STRTAB))
                {
                  stringIdx = six;
                  break;
                }
            }
	  if (six == ehdr.e_shnum)
	    return 0; /* stringIdx not found */

          for (id = 0; id < dc; id++)
            {
              Elf32_Dyn dyn;
              if (0 != getDynTag (data,
                                  size,
                                  &ehdr,
                                  phdr.p_offset, phdr.p_filesz, id, &dyn))
                return 0;
              switch (dyn.d_tag)
                {
                case DT_RPATH:
                  {
                    const char *rpath;

                    rpath = readStringTable (data,
                                             size,
                                             &ehdr,
                                             stringIdx, dyn.d_un.d_val);
                    /* "source" of the dependencies: path
                       to dynamic libraries */
                    if (rpath != NULL)
                      {
                        ADD (rpath, EXTRACTOR_METATYPE_LIBRARY_SEARCH_PATH);
                      }
                    break;
                  }
                case DT_NEEDED:
                  {
                    const char *needed;

                    needed = readStringTable (data,
                                              size,
                                              &ehdr,
                                              stringIdx, dyn.d_un.d_val);
                    if (needed != NULL)
                      {
                        ADD (needed, EXTRACTOR_METATYPE_LIBRARY_DEPENDENCY);
                      }
                    break;
                  }
                }
            }

        }
    }

  return 0;
}
