#include <huxerui/huxerui.h>

#include "counter.generated.h"

using namespace huxerui;

View App() {
  return huxerui_generated::Counter();
}

const Application application{
    App,
    {
        .window = {
            .title = "HuxerUI Declarative Counter",
            .initial_size = {520.0F, 360.0F},
        },
    }
};
