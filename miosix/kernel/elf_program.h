/***************************************************************************
 *   Copyright (C) 2012-2024 by Terraneo Federico                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#pragma once

#include <utility>
#include "elf_types.h"
#include "config/miosix_settings.h"

#ifdef WITH_PROCESSES

namespace miosix {

/**
 * This class represents an elf file.
 */
class ElfProgram
{
public:
    /**
     * Default constructor
     */
    ElfProgram() : elf(nullptr), size(0), valid(false) {}

    /**
     * Constructor
     * \param elf pointer to the elf file's content. Ownership of the data
     * remains of the caller, that is, the pointer is not deleted by this
     * class. This is done to allow passing a pointer directly to a location
     * in the microcontroller's FLASH memory, in order to avoid copying the
     * elf in RAM
     * \param size size in bytes (despite elf is an unsigned int*) of the
     * content of the elf file
     */
    ElfProgram(const unsigned int *elf, unsigned int size);

    /**
     * \return true if this is a valid elf file
     */
    bool isValid() const { return valid; }
    
    /**
     * \return the a pointer to the elf header
     */
    const Elf32_Ehdr *getElfHeader() const
    {
        return reinterpret_cast<const Elf32_Ehdr*>(elf);
    }
    
    /**
     * \return the already relocated value of the entry point 
     */
    unsigned int getEntryPoint() const
    {
        unsigned int base=reinterpret_cast<unsigned int>(elf);
        return base+getElfHeader()->e_entry;
    }
    
    /**
     * \return an array of struct Elf32_Phdr
     */
    const Elf32_Phdr *getProgramHeaderTable() const
    {
        unsigned int base=reinterpret_cast<unsigned int>(elf);
        return reinterpret_cast<const Elf32_Phdr*>(base+getElfHeader()->e_phoff);
    }
    
    /**
     * \return the number of entries in the program header table
     */
    int getNumOfProgramHeaderEntries() const
    {
        return getElfHeader()->e_phnum;
    }
    
    /**
     * \return a number representing the elf base address in memory
     */
    unsigned int getElfBase() const
    {
        return reinterpret_cast<unsigned int>(elf);
    }
    
    /**
     * \return the size in bytes of the elf file, as passed in the constructor
     */
    unsigned int getElfSize() const
    {
        return size;
    }
    
private:
    /**
     * \param size elf file size
     * \return false if the file is not valid
     * \throws runtime_error for selected specific types of errors 
     */
    bool validateHeader();
    
    /**
     * \param dynamic pointer to dynamic segment
     * \param size elf file size
     * \param dataSegmentSize size of data segment in memory
     * \return false if the dynamic segment is not valid
     * \throws runtime_error for selected specific types of errors 
     */
    bool validateDynamicSegment(const Elf32_Phdr *dynamic,
            unsigned int dataSegmentSize);
    
    /**
     * \param x field to check for word alignment issues
     * \return true if not aligned correctly
     */
    static bool isUnaligned4(unsigned int x) { return x & 0b11; }
    
    /**
     * \param x field to check for doubleword alignment issues
     * \return true if not aligned correctly
     */
    static bool isUnaligned8(unsigned int x) { return x & 0b111; }
    
    const unsigned int *elf; ///<Pointer to the content of the elf file
    unsigned int size; ///< Size in bytes of the elf file
    bool valid; ///< All checks passed
};

/**
 * This class represent the RAM image of a process.
 */
class ProcessImage
{
public:
    /**
     * Constructor, creates an empty process image.
     */
    ProcessImage() : image(nullptr), size(0) {}

    /**
     * \return true if this is a valid process image
     */
    bool isValid() const { return image!=nullptr; }
    
    /**
     * Starting from the content of the elf program, create an image in RAM of
     * the process, including copying .data, zeroing .bss and performing
     * relocations
     */
    void load(const ElfProgram& program);
    
    /**
     * \return a pointer to the base of the program image
     */
    unsigned int *getProcessBasePointer() const { return image; }
    
    /**
     * \return the size in bytes (despite getProcessBasePointer() returns an
     * unsigned int*) of the process image, or zero if it is not valid
     */
    unsigned int getProcessImageSize() const { return size; }
    
    /**
     * Destructor. Deletes the process image memory.
     */
    ~ProcessImage();
    
private:
    ProcessImage(const ProcessImage&);
    ProcessImage& operator= (const ProcessImage&);
    
    unsigned int *image; //Pointer to the process image in RAM
    unsigned int size;   //Size in bytes of the process image
};

} //namespace miosix

#endif //WITH_PROCESSES
