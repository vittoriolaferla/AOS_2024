 /***************************************************************************
  *   Copyright (C) 2024 by Terraneo Federico                               *
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
  *   You should have received a copy of the GNU General Public License     *
  *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
  ***************************************************************************/

#pragma once

#include <cstring>
#include <fstream>
#include <list>
#include <cassert>
#include <algorithm>
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>
#include "tree.h"
#include "image.h"
#include "romfs_types.h"

/**
 * \param an unsigned int
 * \return the same int forced into little endian representation
 */
template<typename T>
T toLittleEndian(T x)
{
    using namespace std;
    static bool first=true, little;
    union {
        T a;
        unsigned char b[sizeof(T)];
    } endian;
    if(first)
    {
        endian.a=0x12;
        little=endian.b[0]==0x12;
        first=false;
    }
    if(little) return x;
    endian.a=x;
    for(int i=0;i<sizeof(T)/2;i++)
    {
        swap(endian.b[i],endian.b[sizeof(T)-1-i]);
    }
    return endian.a;
}

auto toLittleEndian16=toLittleEndian<unsigned short>;
auto toLittleEndian32=toLittleEndian<unsigned int>;

/**
 * Create a RomFs image from a directory tree
 */
class MkRomFs
{
public:
    /**
     * Everything is done in the constructor, the class exists as a convenience
     * \param io iostream where the image will be built
     * \param root root of the directory tree
     */
    MkRomFs(std::iostream& io, const FilesystemEntry& root) : img(io)
    {
        // Construct the filesystem header
        RomFsHeader header;
        memset(&header,0,sizeof(RomFsHeader));
        strncpy(header.marker,"wwwww",6);
        strncpy(header.fsName,"RomFs 2.01",11);
        strncpy(header.osName,"Miosix",7);
        //header.imageSize still unknown at this point
        auto headerOffset=img.append(header,romFsStructAlignment);

        // Write the root directory
        RomFsDirectoryEntry rootDir;
        memset(&rootDir,0,sizeof(RomFsDirectoryEntry));
        //rootDir.inode still unknown at this point
        //rootDir.size still unknown at this point
        rootDir.mode=toLittleEndian16(root.mode);
        rootDir.uid=toLittleEndian16(root.uid);
        rootDir.gid=toLittleEndian16(root.gid);
        auto rootOffset=img.append(rootDir,romFsStructAlignment);
        img.appendString(""); //Root dir name is always empty
        auto info=addDirectoryInode(root,0); //prevInode of root dir always 0

        // Go back and update rootDir
        rootDir.inode=toLittleEndian32(info.inode);
        rootDir.size=toLittleEndian32(info.size);
        img.put(rootDir,rootOffset);

        // Final alignment
        img.align(romFsImageAlignment);

        // Go back and update header
        header.imageSize=toLittleEndian32(img.size());
        img.put(header,headerOffset);
    }

    /**
     * \return the image size
     */
    unsigned int size() const { return img.size(); }

private:
    struct InodeInfo
    {
        InodeInfo(unsigned int inode=0, unsigned int size=0)
                : inode(inode), size(size) {}
        unsigned int inode;
        unsigned int size;
    };

    /**
     * Add a directory inode to the image
     * \param dir directory to add
     * \param prevInode inode of the parent directory
     * \return inode added
     */
    InodeInfo addDirectoryInode(const FilesystemEntry& dir, unsigned int prevInode)
    {
        using namespace std;

        assert(dir.isDirectory());

        // Align the directory inode
        img.align(romFsStructAlignment);

        // Write first entry
        RomFsFirstEntry prev;
        memset(&prev,0,sizeof(RomFsFirstEntry));
        prev.parentInode=toLittleEndian32(prevInode);
        auto inode=img.append(prev,romFsStructAlignment);

        // Write entries
        list<unsigned int> entryOffsets;
        for(auto& d : dir.directoryEntries)
        {
            RomFsDirectoryEntry de;
            memset(&de,0,sizeof(RomFsDirectoryEntry));
            //de.inode still unknown at this point
            //de.size still unknown at this point
            de.mode=toLittleEndian16(d.mode);
            de.uid=toLittleEndian16(d.uid);
            de.gid=toLittleEndian16(d.gid);
            entryOffsets.push_back(img.append(de,romFsStructAlignment));
            img.appendString(d.name);
        }

        // Compute the entire directory inode size.
        // NOTE: Must be done before we recursively add the directory content!
        auto size=img.size()-inode; //inode is also address of first byte

        // Then for each entry, recursively add the content
        list<InodeInfo> entryContent;
        for(auto& d : dir.directoryEntries)
        {
            switch(d.mode & S_IFMT)
            {
                case S_IFDIR:
                    entryContent.push_back(addDirectoryInode(d,inode));
                    break;
                case S_IFREG:
                    entryContent.push_back(addFileInode(d));
                    break;
                case S_IFLNK:
                    entryContent.push_back(addSymlinkInode(d));
                    break;
                default:
                    cerr<<d.name<<": unhandled file type found, skip"<<endl;
            }
        }

        // Finally, go back and fill in the missing information
        auto o=begin(entryOffsets);
        auto c=begin(entryContent);
        for(; o!= end(entryOffsets) && c!= end(entryContent); ++o, ++c)
        {
            auto de=img.get<RomFsDirectoryEntry>(*o);
            de.inode=toLittleEndian32(c->inode);
            de.size=toLittleEndian32(c->size);
            img.put(de,*o);
        }
        return InodeInfo(inode,size);
    }

    /**
     * Add a file inode to the image
     * \param dir directory to add
     * \return inode added
     */
    InodeInfo addFileInode(const FilesystemEntry& file)
    {
        assert(file.isFile());
        std::ifstream in(file.path);
        if(!in) throw std::runtime_error(file.path+": file not found");
        auto inode=img.appendStream(in,romFsFileAlignment);

        // Compute the entire directory inode size
        auto size=img.size()-inode; //inode is also address of first byte
        return InodeInfo(inode,size);
    }

    /**
     * Add a symlink inode to the image
     * \param dir directory to add
     * \return inode added
     */
    InodeInfo addSymlinkInode(const FilesystemEntry& link)
    {
        assert(link.isLink());
        //Symlinks are not aligned nor terminated with \0
        auto inode=img.appendString(link.path,false);

        // Compute the entire directory inode size
        auto size=img.size()-inode; //inode is also address of first byte
        return InodeInfo(inode,size);
    }

    Image<unsigned int> img; ///< Backing storage
};
