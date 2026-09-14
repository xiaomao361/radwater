#pragma once
#include "notebook.h"
namespace pond {
inline bool newlyLinked(const Catch& c,uint32_t before,uint32_t after){
    return c.object()?bool(annotationBit(c,after)&~annotationBit(c,before)):
        fishAnnotation(c,after)&&!fishAnnotation(c,before);
}
// Return an exact page, or -1 for no unread content / failed lookup. Caller preserves journal state.
inline int unreadEntry(Journal& journal,const ReadingState& reading,uint32_t known,
                       unsigned& ordinal,Catch& selected,uint32_t before=0xffffffffu){
    for(unsigned pass=0;pass<3;++pass)for(unsigned n=0;n<journal.discoveries;++n){
        Catch c;if(!journal.discovery(n,c))return -1;
        bool note=reading.noteUnread(c,known);
        bool match=pass==0?note&&newlyLinked(c,before,known):pass==1?note:!reading.mainRead(c);
        if(match){ordinal=n;selected=c;return note&&(pass<2)?(c.object()?3:1):reading.entryPage(c,known);}
    }
    return -1;
}
}
