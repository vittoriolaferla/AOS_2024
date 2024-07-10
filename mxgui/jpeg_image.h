#ifndef JPEG_IMAGE_H
#define JPEG_IMAGE_H

#include "image.h"
#include "byte_reader.h"
#include <string>
#include <fstream>
#include <vector> //least painful since we dont now how long the huffmancode are
#include <cmath>
#include <deque>
#include <memory>
#include <mutex>



namespace mxgui {

   


    typedef unsigned char byte;
    typedef unsigned int uint;

    // Start of Frame markers, non-differential, Huffman coding
    const byte SOF0 = 0xC0; // Baseline DCT
    const byte SOF1 = 0xC1; // Extended sequential DCT
    const byte SOF2 = 0xC2; // Progressive DCT
    const byte SOF3 = 0xC3; // Lossless (sequential)

    // Start of Frame markers, differential, Huffman coding
    const byte SOF5 = 0xC5; // Differential sequential DCT
    const byte SOF6 = 0xC6; // Differential progressive DCT
    const byte SOF7 = 0xC7; // Differential lossless (sequential)

    // Start of Frame markers, non-differential, arithmetic coding
    const byte SOF9 = 0xC9;  // Extended sequential DCT
    const byte SOF10 = 0xCA; // Progressive DCT
    const byte SOF11 = 0xCB; // Lossless (sequential)

    // Start of Frame markers, differential, arithmetic coding
    const byte SOF13 = 0xCD; // Differential sequential DCT
    const byte SOF14 = 0xCE; // Differential progressive DCT
    const byte SOF15 = 0xCF; // Differential lossless (sequential)

    // Define Huffman Table(s)
    const byte DHT = 0xC4;

    // JPEG extensions -- never encouternered
    const byte JPG = 0xC8;

    // Define Arithmetic Coding Conditioning(s)
    const byte DAC_JPEG = 0xCC;

    // Restart interval Markers
    const byte RST0 = 0xD0;
    const byte RST1 = 0xD1;
    const byte RST2 = 0xD2;
    const byte RST3 = 0xD3;
    const byte RST4 = 0xD4;
    const byte RST5 = 0xD5;
    const byte RST6 = 0xD6;
    const byte RST7 = 0xD7;

    // Other Markers
    const byte SOI = 0xD8; // Start of Image
    const byte EOI = 0xD9; // End of Image
    const byte SOS = 0xDA; // Start of Scan
    const byte DQT = 0xDB; // Define Quantization Table(s)
    const byte DNL = 0xDC; // Define Number of Lines -- never used skip--
    const byte DRI = 0xDD; // Define Restart Interval
    const byte DHP = 0xDE; // Define Hierarchical Progression-- never used skip
    const byte EXP = 0xDF; // Expand Reference Component(s) -- never used, skipped

    // APPN Markers
    const byte APP0 = 0xE0;
    const byte APP1 = 0xE1;
    const byte APP2 = 0xE2;
    const byte APP3 = 0xE3;
    const byte APP4 = 0xE4;
    const byte APP5 = 0xE5;
    const byte APP6 = 0xE6;
    const byte APP7 = 0xE7;
    const byte APP8 = 0xE8;
    const byte APP9 = 0xE9;
    const byte APP10 = 0xEA;
    const byte APP11 = 0xEB;
    const byte APP12 = 0xEC;
    const byte APP13 = 0xED;
    const byte APP14 = 0xEE;
    const byte APP15 = 0xEF;

    // Misc Markers
    const byte JPG0 = 0xF0; // -- reserved
    const byte JPG1 = 0xF1;
    const byte JPG2 = 0xF2;
    const byte JPG3 = 0xF3;
    const byte JPG4 = 0xF4;
    const byte JPG5 = 0xF5;
    const byte JPG6 = 0xF6;
    const byte JPG7 = 0xF7;
    const byte JPG8 = 0xF8;
    const byte JPG9 = 0xF9;
    const byte JPG10 = 0xFA;
    const byte JPG11 = 0xFB;
    const byte JPG12 = 0xFC;
    const byte JPG13 = 0xFD; //--reserved we will never encountered
    const byte COM = 0xFE;   // skipp
    const byte TEM = 0x01;   // skipp

    struct QuantizationTable
    {
        uint table[64] = {0};
        bool set = false; // True if is popolized
    };

    struct HuffmanTable
    {
        byte offset[17] = {0}; // Here we save the offset of each of the 16 jagged array
        byte symbols[162] = {0};
        uint codes[162] = {0};
        bool set = false;
    };

    struct ColorComponent
    {
        byte horizontalSamplingFactor = 1;
        byte verticalSamplingFactor = 1;
        byte quantizationtableID = 0;
        byte huffmanDCTableID = 0;
        byte huffmanACTableID = 0;
        bool used = false;
    };



    // Here is the data structure for the entire jpeg file (header is not appropriate)
    struct Header
    {
        QuantizationTable quantizationTable[4];
        HuffmanTable huffmanDCTable[4];
        HuffmanTable huffmanACTable[4];

        byte frameType = 0;
        uint height = 0;
        uint width = 0;
        byte numComponents = 0;
        bool zeroBased = false;

        byte startOfSelection = 0;
        byte endOfSelection = 0;
        byte succesiveApproximationHigh = 0;
        byte succesiveApproximationLow = 0;

        std::vector<byte> huffmanData; // least painful way dont knoww the size

        uint reastartInterval = 0;
        ColorComponent colorComponents[3];
        bool valid = true;

        uint mcuHeight = 0;
        uint mcuWidth = 0;
        uint mcuHeightReal = 0;
        uint mcuWidthReal = 0;

        byte horizontalSamplingFactor = 1;
        byte verticalSamplingFactor = 1;
    };

    struct MCU {
        union
        { 
            // same addres in memory with multiple names
            int y[64] = {0};
            int r[64];
        };
        union
        {
            int cb[64] = {0};
            int g[64];
        };
        union
        {

            int cr[64] = {0};
            int b[64];
        };

        int* operator[](uint i ){
            switch (i)
            {
            case 0 : 
                return y;
            case 1:
                return cb;
            case 2:
                return cr;
            default:
                return nullptr;
            }
        }
    };

    // IDCT scaling factors
    const float m0 = 2.0 * std::cos(1.0 / 16.0 * 2.0 * M_PI);
    const float m1 = 2.0 * std::cos(2.0 / 16.0 * 2.0 * M_PI);
    const float m3 = 2.0 * std::cos(2.0 / 16.0 * 2.0 * M_PI);
    const float m5 = 2.0 * std::cos(3.0 / 16.0 * 2.0 * M_PI);
    const float m2 = m0 - m5;
    const float m4 = m0 + m5;

    const float s0 = std::cos(0.0 / 16.0 * M_PI) / std::sqrt(8);
    const float s1 = std::cos(1.0 / 16.0 * M_PI) / 2.0;
    const float s2 = std::cos(2.0 / 16.0 * M_PI) / 2.0;
    const float s3 = std::cos(3.0 / 16.0 * M_PI) / 2.0;
    const float s4 = std::cos(4.0 / 16.0 * M_PI) / 2.0;
    const float s5 = std::cos(5.0 / 16.0 * M_PI) / 2.0;
    const float s6 = std::cos(6.0 / 16.0 * M_PI) / 2.0;
    const float s7 = std::cos(7.0 / 16.0 * M_PI) / 2.0;


    // This is used beacuse the bit are stored following a zig zag map instead that in the normal way
    const byte zigZagMap[] = {
        0, 1, 8, 16, 9, 2, 3, 10,
        17, 24, 32, 25, 18, 11, 4, 5,
        12, 19, 26, 33, 40, 48, 41, 34,
        27, 20, 13, 6, 7, 14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36,
        29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46,
        53, 60, 61, 54, 47, 55, 62, 63};
        

//Do not support sampling factors and progresssive jpeg
class JpegImage : public ImageBase { 
public:
    /**
     * Default constructor
     */
    JpegImage() : ImageBase(100,100), name(0), f(0) {}

    /**
     * Construct from a filename
     * \param filename file name of jpeg image
     */
    explicit JpegImage(const std::string &filename): name(0),
     header(0) {this->open(filename); }

    /**
     * Open a jpeg file
     * \param filename file name of jpeg image
     */
    void open(const std::string &filename);

    /**
     * Close tga file
     */
    void close();

    /**
     * \return true if image is open
     */
    bool isOpen() const { return name!=0; }

     /**
     * Copy constructor
     * \param rhs instance to copy from
     */
    JpegImage(const JpegImage& rhs): name(0) { this->open(rhs.name); }

     /**
     * Operator =
     * \param rhs instance to copy from
     * \return reference to *this
     */
    const JpegImage& operator= (const JpegImage& rhs)
    {
        if(this!=&rhs) this->open(rhs.name);
        return *this;
    }


    /**
     * Get pixels from tha image. This member function can be used to get
     * up to a full horizontal line of pixels from an image.
     * \param p Start point, within <0,0> and <getWidth()-1,getHeight()-1>
     * \param colors pixel data is returned here. Array size must be equal to
     * the length parameter
     * \param length number of pixel to retrieve from the starting point.
     * start.x()+length must be less or equal to getWidth()
     * \return true if success. If false then it means the class does not
     * represent a valid image, or a disk error occurred in case the image
     * is stored on disk.
     */
          virtual bool getScanLine(mxgui::Point p, mxgui::Color colors[], unsigned short length) const;

  
    /**
     * Destructor
     */
    virtual ~JpegImage() { close(); }
    

private:

    char* name;
    mutable int current_height=0;
    FILE *f;
    mutable Header* header;
    mutable int* previousDCs;
    mutable BitReader b;
    mutable int previousIndex = -1;
    mutable int index = 0;
    mutable bool isValid= true;
    mutable bool isFirstLine=true;

    

    // Reading functions
    Header* readJPG(const std::string &filename);
    bool readStartOfFrame(std::ifstream &inFile, Header *const header);
    void readQuantizationTable(std::ifstream &inFile, Header *const header);
    void readHuffmanTable(std::ifstream &inFile, Header *const header);
    void readStartOfScan(std::ifstream &inFile, Header *const header);
    void readRestartInterval(std::ifstream &inFile, Header *const header);
    void readAPPN(std::ifstream &inFile, Header *const header);
    void readComment(std::ifstream &inFile, Header *const header);

    //Decoding functions
    void generateCode(HuffmanTable &hTable);
    byte getNextSymbol(BitReader &b, const HuffmanTable &hTable) const;
    bool decodeMCUComponents(BitReader &b, int *const component, int &previousDC, const HuffmanTable &dcTable, const HuffmanTable &acTable) const;
    void dequantizeMCUComponent(const QuantizationTable& qTable, int* const component) const;
    MCU decodeHuffmanData(Header *const header, MCU mcu, int index, BitReader &bitReader, int previousDCs[3]) const;
    void generateCodes(Header *const header);
    MCU inverseDCT(const Header* const header, MCU  mcu) const;
    void inverseDCTComponent( int* const component) const;
    MCU dequantize(const Header* const header, MCU mcu) const;
    MCU processOneMCU(Header *const header,MCU mcu, int index, BitReader &bitReader, int previousDCs[3]) const;
    MCU YCbCrToRGBMCU(MCU mcu) const;


void reset() const{
    previousDCs = new int[3]{0};
    // Reset other member variables to their initial states
    current_height = 0;
    b.reset(); // This will destroy the managed BitReader object, if any, and reset the unique_ptr
    previousIndex = -1;
    index = 0;
    isFirstLine = true;

}


};
}// namespace mxgui

#endif 