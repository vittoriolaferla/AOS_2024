/***************************************************************************
 *   Copyright (C) 2011 by Terraneo Federico                               *
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

#include "jpeg_image.h"
#include <cstring>
#include <iostream>
#include <deque>
#include <memory>

namespace mxgui
{

    void JpegImage::open(const std::string &filename)
    {
        previousDCs = new int[3]{0};
        this->header = this->readJPG(filename);
        if (header->valid == false) this->isValid=false;
        generateCodes(this->header);
        this->height = this->header->height;
        this->width = this->header->width;
    }


    void JpegImage::close()
    {
        if (this->name == 0)
            return;
        delete[] this->name;
        this->name = 0;
        fclose(this->f);
    }

    bool JpegImage::getScanLine(mxgui::Point p, mxgui::Color colors[], unsigned short length) const
    {   if(!this->isValid) return false;
        const MCU mcu;
        const uint mcuWidth = (header->width + 7) / 8;       // Calculate MCUs per row
        static std::vector<MCU> mcuBuffer(mcuWidth);         // Buffer for a row of MCUs, static to persist across calls
        static std::vector<bool> mcuLoaded(mcuWidth, false); // Track loaded state of MCUs, static to persist across calls
        static uint lastProcessedRow = 0xFFFFFFFF;
        
        if(isFirstLine){
            b.setData(header->huffmanData);
            isFirstLine=false;
        }

        const uint mcuRow = p.y() / 8;
        const uint pixelRow = p.y() % 8;

        // Check if we've moved to a new row of MCUs or if it's the first call
        if (mcuRow != lastProcessedRow / 8)
        {
            std::fill(mcuLoaded.begin(), mcuLoaded.end(), false); // Reset MCU loaded states
            lastProcessedRow = this->current_height;              // Update the last processed row
        }

        for (uint x = 0; x < header->width; ++x)
        {
            const uint mcuColumn = x / 8;
            const uint pixelColumn = x % 8;
            const uint pixelIndex = pixelRow * 8 + pixelColumn;
            const int mcuIndex = mcuRow * mcuWidth + mcuColumn;


            // Load MCU into buffer if not already loaded for this row of MCUs
           if (!mcuLoaded[mcuColumn])
            {
                mcuBuffer[mcuColumn] = processOneMCU(header,mcu, mcuIndex, b,previousDCs);
                mcuLoaded[mcuColumn] = true; // Mark as loaded
            }

            const MCU &mcu = mcuBuffer[mcuColumn];

            // Perform color conversion and store in the colors array
            //std::cout << "R value: " << mcu.r[pixelIndex] << "   G value  " << mcu.g[pixelIndex] << "   B value  " << mcu.b[pixelIndex] << "\n";
            colors[x] = ((mcu.r[pixelIndex] & 0xF8) << 8) | ((mcu.g[pixelIndex] & 0xFC) << 3) | (mcu.b[pixelIndex] >> 3);
        }
        
        this->current_height++;
        if(this->current_height == this->height){
            std::cout <<"Start resetting";
            reset();
        }



        return true;
    }

    void JpegImage::readQuantizationTable(std::ifstream &inFile, Header *const header)
    {
        std::cout << "reading DQT marker\n";
        // Legge due byte dal file 'inFile' e li combina per creare un valore intero a 16 bit.
        // La prima chiamata 'inFile.get()' legge i primi 8 bit e restituisce un valore non segnato.
        // L'operatore '<< 8' sposta questi 8 bit a sinistra di 8 posizioni (moltiplicando per 256).
        // La seconda chiamata 'inFile.get()' legge i successivi 8 bit (i meno significativi).
        // Questi due valori vengono sommati per ottenere il valore intero a 16 bit 'length'.
        int length = (inFile.get() << 8) + inFile.get();
        length -= 2;

        while (length > 0)
        {
            byte tableInfo = inFile.get();
            length -= 1;
            byte tableID = tableInfo & 0x0F;

            if (tableID > 3)
            {
                std::cout << "Error - Invalid quantization table ID:" << (uint)tableID << '\n';
                header->valid = false;
                return;
            }
            header->quantizationTable[tableID].set = true;
            // shift of 4 bits
            if (tableInfo >> 4 != 0)
            {
                for (uint i = 0; i < 64; ++i)
                { // here we read 2 bytes of the file, in the else we read just one bit
                    header->quantizationTable[tableID].table[zigZagMap[i]] = (inFile.get() << 8) + inFile.get();
                }
                length -= 128;
            }
            else
            {
                for (uint i = 0; i < 64; ++i)
                {
                    // Basically read one byte at the time and store it
                    header->quantizationTable[tableID].table[zigZagMap[i]] = +inFile.get();
                }
                length -= 64;
            }
        }

        if (length != 0)
        {
            std::cout << "Error - DQT invalid \n";
            header->valid = false;
        }
    }

    bool JpegImage::readStartOfFrame(std::ifstream &inFile, Header *const header)
    {

        std::cout << "Reading SOF Marker\n";
        if (header->numComponents != 0)
        {
            std::cout << "Error - Multiple SOFs detected\n";
            header->valid = false;
            return false;
        }

        uint length = (inFile.get() << 8) + inFile.get();

        byte precision = inFile.get();
        if (precision != 8)
        {
            std::cout << "error - Invalid precsion " << (uint)precision << '\n';
            header->valid = false;
            return false;
        }

        header->height = (inFile.get() << 8) + inFile.get();
        header->width = (inFile.get() << 8) + inFile.get();
        if (header->height == 0 || header->width == 0)
        {
            std::cout << "Error - Invalid dimension\n";
            header->valid = false;
            return false;
        }
        header->numComponents = inFile.get();
        if (header->numComponents == 4)
        {
            std::cout << "Error - CMYK color mode not supported\n";
            header->valid = false;
            return false;
        }
        if (header->numComponents == 0)
        {
            std::cout << "Error - Number of color components must not be zero\n";
            header->valid = false;
            return false;
        }

        for (uint i = 0; i < header->numComponents; ++i)
        {
            byte componentID = inFile.get();
            // components IDs are usually 1,2,3 but rarely can be seen as 0,1,2
            // always forche them into 1,2,3 for consitency
            if (componentID == 0)
            {
                header->zeroBased = true;
            }
            if (header->zeroBased)
            {
                componentID += 1;
            }
            if (componentID == 4 || componentID == 5)
            {
                std::cout << "Error - YIQ color mode not supported\n";
                header->valid = false;
                return false;
            }
            if (componentID == 0 || componentID > 3)
            {
                std::cout << "Error - Invalid component ID\n"
                          << (uint)componentID << "\n";
                header->valid = false;
                return false;
            }
            ColorComponent *component = &header->colorComponents[componentID - 1];
            if (component->used)
            {
                std::cout << "Error - Duplicate color component\n"
                          << (uint)componentID << "\n";
                header->valid = false;
                return false;
            }

            component->used = true;
            byte samplingFactor = inFile.get();
            component->horizontalSamplingFactor = samplingFactor >> 4;

            if (component->horizontalSamplingFactor != 1 || component->verticalSamplingFactor != 1)
            {
                std::cout << "Error - Error sampling factors not supported\n"
                          << "\n";
                header->valid = false;
                return false;
            }
            component->verticalSamplingFactor = samplingFactor & 0x0F;
            component->quantizationtableID = inFile.get();
            if (component->quantizationtableID > 3)
            {
                std::cout << "Error - Invalid quantization table ID in frame components\n"
                          << (uint)componentID << "\n";
                header->valid = false;
                return false;
            }
            // The length that we read at the beginnig must match where we are ( minus 8 byte for precison.. and 3 byte for every color channel)
            if (length - 8 - (3 * header->numComponents) != 0)
            {
                std::cout << "Error - SDF invalids\n";
                header->valid = false;
                return false;
            }
        }
        return true;
    }

    void JpegImage::readRestartInterval(std::ifstream &inFile, Header *const header)
    {
        std::cout << "Reading DRI Marker\n";
        uint length = (inFile.get() << 8) + inFile.get();
        header->reastartInterval = (inFile.get() << 8) + inFile.get();
        if (length - 4 != 0)
        {
            std::cout << "error - DRI invalid\n";
            header->valid = false;
        }
    }

    void JpegImage::readHuffmanTable(std::ifstream &inFile, Header *const header)
    {
        std::cout << "Reading DHT Marker\n";
        int length = (inFile.get() << 8) + inFile.get();
        length -= 2;

        while (length > 0)
        {
            byte tableInfo = inFile.get();
            byte tableID = tableInfo & 0x0F; // Is between 0 and 3
            bool ACTable = tableInfo >> 4;   // Is either 0 or 1

            if (tableID > 3)
            {
                std::cout << "Error - Invalid Huffman table ID: " << (uint)tableID << '\n';
                header->valid = false;
                return;
            }

            HuffmanTable *hTable;

            if (ACTable)
            {
                hTable = &header->huffmanACTable[tableID];
            }
            else
            {
                hTable = &header->huffmanDCTable[tableID];
            }
            hTable->set = true;

            hTable->offset[0] = 0;
            uint allSymbols = 0;
            // read and store the position of the length of the 16 symbols
            for (uint i = 1; i <= 16; ++i)
            {
                allSymbols += inFile.get();
                hTable->offset[i] = allSymbols;
            }
            if (allSymbols > 162)
            {
                std::cout << "Error - Too many symbols in Huffman table\n";
                header->valid = false;
                return;
            }

            for (uint i = 0; i < allSymbols; ++i)
            {
                hTable->symbols[i] = inFile.get();
                // save all the symbols of the Huffman Table
            }

            length -= 17 + allSymbols;
        }
        if (length != 0)
        {
            std::cout << "Error - Too many simbols in Huffman table \n";
        }
    }

    void JpegImage::readStartOfScan(std::ifstream &inFile, Header *const header)
    {
        std::cout << "Reading SOS Marker \n";
        if (header->numComponents == 0)
        {
            std::cout << "Error - Too many simbols in Huffman table \n";
            header->valid = false;
            return;
        }

        uint length = (inFile.get() << 8) + inFile.get();

        for (uint i = 0; i < header->numComponents; i++)
        {
            header->colorComponents[i].used = false;
        }

        byte numComponents = inFile.get();
        for (uint i = 0; i < numComponents; i++)
        {
            byte componentID = inFile.get();
            // component ID are usually 1,2,3 but rarely can be seen as 0,1,2
            if (header->zeroBased)
            {
                componentID += 1;
            }
            if (componentID > header->numComponents)
            {
                std::cout << "Error - Invalid color component ID: \n"
                          << (uint)componentID << '\n';
                header->valid = false;
                return;
            }
            ColorComponent *component = &header->colorComponents[componentID - 1];
            if (component->used)
            {
                std::cout << "Error - Duplicate color component ID: \n"
                          << (uint)componentID << '\n';
                header->valid = false;
                return;
            }
            component->used;

            byte huffManTableIDs = inFile.get();
            component->huffmanDCTableID = huffManTableIDs >> 4;
            component->huffmanACTableID = huffManTableIDs & 0x0F;
            if (component->huffmanDCTableID > 3)
            {
                std::cout << "Error - Invalid Huffman AC table ID: \n"
                          << (uint)componentID << '\n';
                header->valid = false;
                return;
            }
            if (component->huffmanDCTableID > 3)
            {
                std::cout << "Error - Invalid Huffman DC table ID:: \n"
                          << (uint)componentID << '\n';
                header->valid = false;
                return;
            }
        }

        header->startOfSelection = inFile.get();
        header->endOfSelection = inFile.get();
        byte succesiveApproximation = inFile.get();
        byte succesiveApproximationHigh = succesiveApproximation >> 4;
        header->succesiveApproximationLow = succesiveApproximation & 0x0F;
    }

    // skip all APPN
    void JpegImage::readAPPN(std::ifstream &inFile, Header *const header)
    {
        std::cout << "reading APPN Marker \n";
        uint length = (inFile.get() << 8) + inFile.get();

        for (uint i = 0; i < length - 2; ++i)
        {
            inFile.get();
            // We read all the part of the file we are not interested so the last c
            // charchter we read is the first marker of the QUANTIZATION TABLE
        }
    }

    void JpegImage::readComment(std::ifstream &inFile, Header *const header)
    {
        std::cout << "Reading COM Marker\n";
        uint length = (inFile.get() << 8) + inFile.get();
        if (length < 2)
        {
            std::cout << "Error - COM invalid\n";
            header->valid = false;
            return;
        }

        for (uint i = 0; i < length - 2; ++i)
        {
            byte read = inFile.get();
            std::cout << "Reading comment: " << read << "\n";
        }
    }

    // Implementation of readJPG
    Header *JpegImage::readJPG(const std::string &filename)
    {
        // Apri un file JPG specificato da 'filename' e leggi i dati dell'immagine qui
        // Creazione di un oggetto 'std::ifstream' chiamato 'inFile'.
        // Questo oggetto verrà utilizzato per leggere dati da un file specificato da 'filename'.
        std::ifstream inFile = std::ifstream(filename, std::ios::in | std::ios::binary);
        // 'filename' è il nome del file che verrà aperto per la lettura. Assicurati che contenga il percorso completo o relativo al file.
        // La modalità 'std::ios::in' indica che il file verrà aperto in modalità di lettura, consentendo la lettura dei dati dal file.
        // La modalità 'std::ios::binary' indica che il file verrà aperto in modalità binaria, ideale per la lettura di file binari come le immagini.
        if (!inFile.is_open())
        {
            std::cout << "Error - Error opening input file\n";
        }
        // Crea un nuovo puntatore di tipo 'Header' chiamato 'header' e assegna a esso un nuovo oggetto di tipo 'Header' allocato sulla memoria heap.
        Header *header = new (std::nothrow) Header;
        // L'uso di 'new' consente di allocare memoria dinamicamente sulla heap per un nuovo oggetto di tipo 'Header'.
        // 'std::nothrow' è utilizzato per gestire eventuali errori di allocazione di memoria senza generare eccezioni.
        // 'header' è ora un puntatore che può essere utilizzato per accedere e manipolare l'oggetto 'Header' allocato dinamicamente.
        // Assicurati di deallocare correttamente questa memoria quando non ne hai più bisogno utilizzando 'delete' per evitare perdite di memoria.
        if (header == nullptr)
        {
            std::cout << "Error - Memory error \n";
            inFile.close();
            return nullptr;
        }
        byte last = inFile.get();
        byte current = inFile.get();
        if (last != 0xFF || current != SOI)
        {
            // Non è un file JPEG valido
            header->valid = false;
            return header;
        }
        last = inFile.get();
        current = inFile.get();
        while (header->valid)
        {
            if (!inFile)
            {
                std::cout << "Error - file ended premature\n";
                header->valid = false;
                inFile.close();
                return header;
            }
            if (last != 0xFF)
            {
                std::cout << "Error - Expected a marker\n";
                header->valid = false;
                inFile.close();
                return header;
            }
            if (current == SOF0)
            {
                header->frameType = SOF0;
                if(!readStartOfFrame(inFile, header))
                {
                    std::cout <<"Impossible to process file due to marker not supported\n";
                    header->valid=false;
                     // Properly throw a runtime_error exception
                    throw std::runtime_error("Marker not supported");
                    return header;
                }
            }

            else if (current == DQT)
            {
                readQuantizationTable(inFile, header);
            }
            else if (current == DHT)
            {
                readHuffmanTable(inFile, header);
            }
            else if (current == SOS)
            {
                readStartOfScan(inFile, header);
                break;
            }
            else if (current == DRI)
            {
                readRestartInterval(inFile, header);
            }
            else if (current >= APP0 && current <= APP15)
            {
                readAPPN(inFile, header);
            }
            else if (current == COM)
            {
                readComment(inFile, header);
            }
            // unused markers can be skipped
            else if ((current >= JPG0 && current <= JPG13) ||
                     current == DNL ||
                     current == DHP ||
                     current == EXP)
            {
                readComment(inFile, header);
            }

            else if (current == TEM)
            {
                // TEM has no size
            }
            else if (current == 0xFF)
            {
                current = inFile.get();
                continue;
            }
            // After beginni g in the file no SOI --not supoorted
            else if (current == SOI)
            {
                std::cout << "Error - Embedded JPGs not supported\n";
                header->valid = false;
                return header;
            }
            else if (current == EOI)
            {
                std::cout << "Error - EOI detected before SOS\n";
                header->valid = false;
                return header;
            }
            // Just hoffman coding
            else if (current == DAC_JPEG)
            {
                std::cout << "Error - Arithmetic Coding mode not supported\n";
                header->valid = false;
                return header;
            }
            //
            else if (current >= SOF0 && current <= SOF15)
            {
                std::cout << "Error - SOF marker not supported: 0x" << std::hex << (uint)current << std::dec << '\n';
                header->valid = false;
                 // Properly throw a runtime_error exception
                throw std::runtime_error("Error - SOF marker not supported");
                return header;
            }
            else if (current >= RST0 && current <= RST7)
            {
                std::cout << "Error - RSTN detected before SOS\n";
                header->valid = false;
                return header;
            }
            else
            {
                std::cout << "Error - Unknown marker: 0x" << std::hex << (uint)current << std::dec << '\n';
                header->valid = false;
                throw std::runtime_error("Error - Unknown marker: ");
                return header;
            }

            last = inFile.get();
            current = inFile.get();
        }

        if (header->valid)
        {
            current = inFile.get();
            // read compressed image dataa
            while (true)
            {

                if (!inFile)
                {
                    std::cout << "Error - File ended prematurely";
                    header->valid = false;
                    inFile.close();
                    return header;
                }

                last = current;
                current = inFile.get();
                if (last == 0xFF)
                {

                    // end of image
                    if (current == EOI)
                    {
                        break;
                    }
                    // 0xFF00 means put a literal 0xFF in image data and ignore 0x00
                    else if (current == 0x00)
                    {
                        header->huffmanData.push_back(last);
                        // overwrite 0x00 with next byte
                        current = inFile.get();
                    }
                    // Restart marker
                    else if (current >= RST0 && current <= RST7)
                    {
                        // overwrite marker with nextByte
                        current = inFile.get();
                    }
                    // ignore multiple 0xFFF in a row
                    else if (current == 0xFF)
                    {
                        continue;
                    }
                    else
                    {
                        std::cout << "Error - Unknown marker: 0x" << std::hex << (uint)current << std::dec << '\n';
                        header->valid = false;
                        return header;
                    }
                    // its not last==0xFF
                }
                else
                {
                    header->huffmanData.push_back(last);
                }
            }
        }

        std::cout << " Jpeg file read!"
                  << "\n";
        // validate Header info
        if (header->numComponents != 1 && header->numComponents != 3)
        {
            std::cout << "Error - " << (uint)header->numComponents << "Color components given (1 or 3 required)\n";
            header->valid = false;
            inFile.close();
            return header;
        }

        for (uint i = 0; i < header->numComponents; ++i)
        {
            if (header->quantizationTable[header->colorComponents[i].quantizationtableID].set == false)
            {
                std::cout << "Error - Color component using unitialized quantization table \n";
                header->valid = false;
                inFile.close();
                return header;
            }
            if (header->quantizationTable[header->colorComponents[i].huffmanACTableID].set == false)
            {
                std::cout << "Error - Color component using unitialized Huffman AC table \n";
                header->valid = false;
                inFile.close();
                return header;
            }
            if (!header->quantizationTable[header->colorComponents[i].huffmanDCTableID].set)
            {
                std::cout << "Error - Color component using unitialized Huffman DC table \n";
                header->valid = false;
                inFile.close();
                return header;
            }
        }
        inFile.close();
        return header;
    }

    // generate all Huffman codes based on symbols from a Huffman table
    void JpegImage::generateCode(HuffmanTable &hTable)
    {
        uint code = 0;
        for (uint i = 0; i < 16; ++i)
        {
            for (uint j = hTable.offset[i]; j < hTable.offset[i + 1]; ++j)
            {
                // We read the code following the offset and we save the code respectevly
                hTable.codes[j] = code;
                code += 1;
            }
            code <<= 1; // binary shifting on left (adding a zero=)
        }
    }

    // return the symbol from the Huffman table that corresponds to
    //  the next Huffman code read from the BitReader
    // Leggi fino a 16 bit e cerchi se esiste un codice con quella lunghezza
    byte JpegImage::getNextSymbol(BitReader &b, const HuffmanTable &hTable) const
    {
        // We read just one bit, we compare the bit with all the codes represented in one bit
        // if we find the match return the corresponding symbol, if e dont we add another bit and repeat procedure
        uint currentCode = 0;
        for (uint i = 0; i < 16; ++i)
        {

            int bit = b.readBit();
            if (bit == -1)
            {
                std::cout << "Error - FF is an invalid symbol\n";
                // FF (255) is not a valid symbol, so we return -1 as error
                return -1;
            }
            currentCode = (currentCode << 1) | bit; // Our current code will shift of one bit and OR with the bit
            for (uint j = hTable.offset[i]; j < hTable.offset[i + 1]; ++j)
            {
                if (currentCode == hTable.codes[j])
                {
                    return hTable.symbols[j];
                }
            }
        }

        std::cout << "Error - No matching for the symbols\n";
        return -1;
    }

    bool JpegImage::decodeMCUComponents(BitReader &b, int *const component, int &previousDC, const HuffmanTable &dcTable, const HuffmanTable &acTable) const
    {
        // Use the DC table to extract the DC coefficient of this component
        // Use the AC table to extarct all the 63 AC coefficient
        byte length = getNextSymbol(b, dcTable);
        // FILE *file = fopen("output_noSub.txt", "a");
        // fprintf(file,  "Simbol :   %d\n", static_cast<int>(length));

        if (length == (byte)-1)
        { // Se è -1 perchè getNextSymbol non ha trovato il simbolo
            std::cout << "Error - Invalid DC value\n";
            return false;
        }
        if (length > 11)
        {
            std::cout << "Error - DC coefficient length greater than 11\n";
            return false;
        }

        int coeff = b.readBits(length);
        if (coeff == -1)
        {
            std::cout << "Error - Invalid DC value\n";
            return false;
        }
        if (length != 0 && coeff < (1 << (length - 1)))
        { // coefficient must be less than 2 ^ length-1
            coeff -= (1 << length) - 1;
        }

        component[0] = coeff + previousDC;
        previousDC = component[0]; // DC coefficient need to be relative to the previous coefricient

        // get the AC values for this MCU component
        uint i = 1;
        while (i < 64)
        {
            byte symbol = getNextSymbol(b, acTable);

            if (symbol == (byte)-1)
            {
                std::cout << "Error - Invalid AC value\n";
                return false;
            }
            // thtas what different from DC table
            //  symbol 0x00 means fill reminer of component with 0
            if (symbol == 0x00)
            {
                for (; i < 64; ++i)
                {
                    component[zigZagMap[i]] = 0;
                }
                return true;
            }

            // otherwise, read next component coefficient
            byte numZeros = symbol >> 4;
            byte coeffLength = symbol & 0x0F;
            coeff = 0;
            // symbol 0xF0 means skip 16 0's
            if (symbol == 0xF0)
            {
                numZeros = 16;
            }

            if (i + numZeros >= 64)
            {
                std::cout << "Error - Zero run-length exceeded MCU\n";
                return false;
            }
            for (uint j = 0; j < numZeros; ++j, ++i)
            {
                component[zigZagMap[i]] = 0;
            }

            if (coeffLength > 10)
            {
                std::cout << "Error - AC coefficient length grater than 10\n";
                return false;
            }

            if (coeffLength != 0)
            {
                coeff = b.readBits(coeffLength);
                if (coeff == -1)
                {
                    std::cout << "Error - Invalid AC value\n";
                    return false;
                }
                if (coeff < (1 << (coeffLength - 1)))
                {
                    coeff -= (1 << coeffLength) - 1;
                }
                component[zigZagMap[i]] = coeff;
                i += 1;
            }
        }

        // get the AC values for this MCU component
        return true;
    }

    void JpegImage::generateCodes(Header *const header)
    {
        // For all the huffman tables
        for (uint i = 0; i < 4; ++i)
        {
            if (header->huffmanDCTable[i].set)
            {
                generateCode(header->huffmanDCTable[i]);
            }
            if (header->huffmanACTable[i].set)
            {
                generateCode(header->huffmanACTable[i]);
            }
        }
    }

    MCU JpegImage::decodeHuffmanData(Header *const header, MCU mcu, int index, BitReader &bitReader, int previousDCs[3]) const
    {
        if (header->reastartInterval != 0 && index % header->reastartInterval == 0)
        {
            previousDCs[0] = 0;
            previousDCs[1] = 0;
            previousDCs[2] = 0;
            bitReader.align();
        }

        for (uint j = 0; j < header->numComponents; ++j)
        {
            if (!decodeMCUComponents(bitReader,
                                     mcu[j],
                                     previousDCs[j],
                                     header->huffmanDCTable[header->colorComponents[j].huffmanDCTableID],
                                     header->huffmanACTable[header->colorComponents[j].huffmanACTableID]))
            {
                std::cout << "Failed to decode MCU component"
                          << "\n";
                return mcu;
            }
        }

        return mcu;
    }

    void JpegImage::dequantizeMCUComponent(const QuantizationTable &qTable, int *const component) const
    {
        for (uint i = 0; i < 64; ++i)
        {
            component[i] *= qTable.table[i];
        }
    }

    MCU JpegImage::dequantize(const Header *const header, MCU mcu) const
    {
        for (uint j = 0; j < header->numComponents; ++j)
        {
            dequantizeMCUComponent(
                header->quantizationTable[header->colorComponents[j].quantizationtableID],
                mcu[j]);
        }

        return mcu;
    }

    // We want to follow the optimizarion of the Inverse transform by using row column decomposition, using AAN

    // There are two more optimization that could be done
    void JpegImage::inverseDCTComponent(int *const component) const
    {
        for (uint i = 0; i < 8; ++i)
        {
            const float g0 = component[0 * 8 + i] * s0;
            const float g1 = component[4 * 8 + i] * s4;
            const float g2 = component[2 * 8 + i] * s2;
            const float g3 = component[6 * 8 + i] * s6;
            const float g4 = component[5 * 8 + i] * s5;
            const float g5 = component[1 * 8 + i] * s1;
            const float g6 = component[7 * 8 + i] * s7;
            const float g7 = component[3 * 8 + i] * s3;

            const float f0 = g0;
            const float f1 = g1;
            const float f2 = g2;
            const float f3 = g3;
            const float f4 = g4 - g7;
            const float f5 = g5 + g6;
            const float f6 = g5 - g6;
            const float f7 = g4 + g7;

            const float e0 = f0;
            const float e1 = f1;
            const float e2 = f2 - f3;
            const float e3 = f2 + f3;
            const float e4 = f4;
            const float e5 = f5 - f7;
            const float e6 = f6;
            const float e7 = f5 + f7;
            const float e8 = f4 + f6;

            const float d0 = e0;
            const float d1 = e1;
            const float d2 = e2 * m1;
            const float d3 = e3;
            const float d4 = e4 * m2;
            const float d5 = e5 * m3;
            const float d6 = e6 * m4;
            const float d7 = e7;
            const float d8 = e8 * m5;

            const float c0 = d0 + d1;
            const float c1 = d0 - d1;
            const float c2 = d2 - d3;
            const float c3 = d3;
            const float c4 = d4 + d8;
            const float c5 = d5 + d7;
            const float c6 = d6 - d8;
            const float c7 = d7;
            const float c8 = c5 - c6;

            const float b0 = c0 + c3;
            const float b1 = c1 + c2;
            const float b2 = c1 - c2;
            const float b3 = c0 - c3;
            const float b4 = c4 - c8;
            const float b5 = c8;
            const float b6 = c6 - c7;
            const float b7 = c7;

            component[0 * 8 + i] = b0 + b7;
            component[1 * 8 + i] = b1 + b6;
            component[2 * 8 + i] = b2 + b5;
            component[3 * 8 + i] = b3 + b4;
            component[4 * 8 + i] = b3 - b4;
            component[5 * 8 + i] = b2 - b5;
            component[6 * 8 + i] = b1 - b6;
            component[7 * 8 + i] = b0 - b7;
        }

        for (uint i = 0; i < 8; ++i)
        {
            const float g0 = component[i * 8 + 0] * s0;
            const float g1 = component[i * 8 + 4] * s4;
            const float g2 = component[i * 8 + 2] * s2;
            const float g3 = component[i * 8 + 6] * s6;
            const float g4 = component[i * 8 + 5] * s5;
            const float g5 = component[i * 8 + 1] * s1;
            const float g6 = component[i * 8 + 7] * s7;
            const float g7 = component[i * 8 + 3] * s3;

            const float f0 = g0;
            const float f1 = g1;
            const float f2 = g2;
            const float f3 = g3;
            const float f4 = g4 - g7;
            const float f5 = g5 + g6;
            const float f6 = g5 - g6;
            const float f7 = g4 + g7;

            const float e0 = f0;
            const float e1 = f1;
            const float e2 = f2 - f3;
            const float e3 = f2 + f3;
            const float e4 = f4;
            const float e5 = f5 - f7;
            const float e6 = f6;
            const float e7 = f5 + f7;
            const float e8 = f4 + f6;

            const float d0 = e0;
            const float d1 = e1;
            const float d2 = e2 * m1;
            const float d3 = e3;
            const float d4 = e4 * m2;
            const float d5 = e5 * m3;
            const float d6 = e6 * m4;
            const float d7 = e7;
            const float d8 = e8 * m5;

            const float c0 = d0 + d1;
            const float c1 = d0 - d1;
            const float c2 = d2 - d3;
            const float c3 = d3;
            const float c4 = d4 + d8;
            const float c5 = d5 + d7;
            const float c6 = d6 - d8;
            const float c7 = d7;
            const float c8 = c5 - c6;

            const float b0 = c0 + c3;
            const float b1 = c1 + c2;
            const float b2 = c1 - c2;
            const float b3 = c0 - c3;
            const float b4 = c4 - c8;
            const float b5 = c8;
            const float b6 = c6 - c7;
            const float b7 = c7;

            component[i * 8 + 0] = b0 + b7 + 0.5f;
            component[i * 8 + 1] = b1 + b6 + 0.5f;
            component[i * 8 + 2] = b2 + b5 + 0.5f;
            component[i * 8 + 3] = b3 + b4 + 0.5f;
            component[i * 8 + 4] = b3 - b4 + 0.5f;
            component[i * 8 + 5] = b2 - b5 + 0.5f;
            component[i * 8 + 6] = b1 - b6 + 0.5f;
            component[i * 8 + 7] = b0 - b7 + 0.5f;
        }
    }

    MCU JpegImage::inverseDCT(const Header *const header, MCU mcu) const
    {

        for (uint j = 0; j < header->numComponents; ++j)
        {
            inverseDCTComponent(mcu[j]);
        }
        return mcu;
    }

    MCU JpegImage::YCbCrToRGBMCU(MCU mcu) const
    {
        for (uint i = 0; i < 64; ++i)
        {
            int r = mcu.y[i] + 1.40f * mcu.cr[i] + 128;
            int g = mcu.y[i] - 0.344f * mcu.cb[i] - 0.714f * mcu.cr[i] + 128;
            int b = mcu.y[i] + 1.722f * mcu.cb[i] + 128;

            if (r < 0)
                r = 0;
            if (r > 225)
                r = 255;
            if (g < 0)
                g = 0;
            if (g > 255)
                g = 255;
            if (b < 0)
                b = 0;
            if (b > 225)
                b = 255;
            mcu.r[i] = r;
            mcu.b[i] = b;
            mcu.g[i] = g;
        }
        return mcu;
    }

    MCU JpegImage::processOneMCU(Header *const header, MCU mcu, int index, BitReader &bitReader, int previousDCs[3]) const
    {

        mcu = decodeHuffmanData(header, mcu, index, bitReader, previousDCs);
        mcu = dequantize(header, mcu);
        mcu = inverseDCT(header, mcu);
        mcu = YCbCrToRGBMCU(mcu);
        index++;
        return mcu;
    }

} // namespace mxgui