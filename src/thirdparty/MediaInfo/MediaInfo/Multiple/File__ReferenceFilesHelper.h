/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef File__ReferenceFilesHelperH
#define File__ReferenceFilesHelperH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/File__Analyze.h"
#include <vector>
//---------------------------------------------------------------------------

namespace MediaInfoLib
{

//***************************************************************************
// Class File__ReferenceFilesHelper
//***************************************************************************

class File__ReferenceFilesHelper
{
public :
    //In
    struct reference
    {
        ZtringList          FileNames;
        Ztring              Source; //Source file name (relative path)
        stream_t            StreamKind;
        size_t              StreamPos;
        size_t              MenuPos;
        int64u              StreamID;
        float64             FrameRate;
        int64u              Delay;
        int64u              FileSize;
        bool                IsCircular;
        bool                IsMain;
        size_t              State;
        std::map<std::string, Ztring> Infos;
        MediaInfo_Internal* MI;
        #if MEDIAINFO_FILTER
            int64u          Enabled;
        #endif //MEDIAINFO_FILTER
        #if MEDIAINFO_NEXTPACKET
            std::bitset<32> Status;
            #if MEDIAINFO_IBI
                ibi::stream IbiStream;
            #endif //MEDIAINFO_IBI
        #endif //MEDIAINFO_NEXTPACKET && MEDIAINFO_IBI

        reference()
        {
            FileNames.Separator_Set(0, __T(","));
            StreamKind=Stream_Max;
            StreamPos=(size_t)-1;
            MenuPos=(size_t)-1;
            StreamID=(int64u)-1;
            FrameRate=0;
            Delay=0;
            FileSize=(int64u)-1;
            IsCircular=false;
            IsMain=false;
            State=0;
            MI=NULL;
            #if MEDIAINFO_FILTER
                Enabled=true;
            #endif //MEDIAINFO_FILTER
        }
    };
    typedef std::vector<reference>  references;
    references                      References;
    bool                            TestContinuousFileNames;
    bool                            ContainerHasNoId;
    bool                            HasMainFile;
    int64u                          ID_Max;

    //Streams management
    void ParseReferences();

    //Constructor / Destructor
    File__ReferenceFilesHelper(File__Analyze* MI, MediaInfo_Config_MediaInfo* Config);
    ~File__ReferenceFilesHelper();

    #if MEDIAINFO_SEEK
    size_t Read_Buffer_Seek (size_t Method, int64u Value, int64u ID);
    #endif //MEDIAINFO_SEEK

private :
    //Streams management
    void ParseReference ();
    void ParseReference_Finalize ();
    void ParseReference_Finalize_PerStream ();
    void Open_Buffer_Unsynch() {Read_Buffer_Unsynched();}

    //Buffer - Global
    void Read_Buffer_Unsynched();

    //temp
    File__Analyze*                  MI;
    MediaInfo_Config_MediaInfo*     Config;
    references::iterator            Reference;
    bool                            Init_Done;
    bool                            Demux_Interleave;
    size_t                          CountOfReferencesToParse;
    float64                         FrameRate;
    float64                         Duration;
    stream_t                        StreamKind_Last;
    size_t                          StreamPos_From;
    size_t                          StreamPos_To;
    #if MEDIAINFO_NEXTPACKET
        int64u                      DTS_Minimal;
        int64u                      DTS_Interval;
    #endif //MEDIAINFO_NEXTPACKET

    //Helpers
    size_t Stream_Prepare(stream_t StreamKind, size_t StreamPos=(size_t)-1);
    void   FileSize_Compute();
    #if MEDIAINFO_EVENTS
    void SubFile_Start();
    int64u                          StreamID_Previous;
    #endif //MEDIAINFO_EVENTS
};

} //NameSpace

#endif

