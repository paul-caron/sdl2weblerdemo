#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <emscripten.h>
#include <emscripten/fetch.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <cstdlib>
#include <atomic>
#include <mutex>
#include <cmath>
#include <functional>





//storage
class Assets
{
public:

static std::map<std::string, SDL_Surface *> images ;
static std::map<std::string, TTF_Font *> fonts ;
static std::vector<const char *> imgUrls ;
static std::vector<const char *> ttfUrls ;
static std::atomic<int> loadedCount ;

static Assets& init()
{
    static Assets INSTANCE {} ;
    return INSTANCE ;
}

static bool fullyLoaded()
{
    int total = ttfUrls.size() + imgUrls.size() ; 
    return loadedCount == total ; 
}

private:

Assets()
{
    fetchImages() ;
    fetchFonts() ;
}
~Assets()
{
    std::cout << "Deleting assets" << std::endl ;
    for(auto & img: images)
    {
        SDL_FreeSurface(img.second) ;
    }
    for(auto & font: fonts)
    {
        TTF_CloseFont(font.second) ;
    }
}

static void fetchImages()
{ 
    for(const auto & url: imgUrls)
    {
        fetchImage(url) ;
    }
}

static void fetchImage(const char * url)
{
    emscripten_fetch_attr_t imgAttr ;
    emscripten_fetch_attr_init(&imgAttr) ;
    strcpy(imgAttr.requestMethod, "GET") ;
    imgAttr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY ;
    imgAttr.onsuccess = imgDownloadSucceeded ;
    imgAttr.onerror = downloadFailed ;
    imgAttr.onprogress = downloadProgress ;
    emscripten_fetch(&imgAttr, url) ;
}

static void fetchFonts()
{ 
    for(const auto & url: ttfUrls)
    {
        fetchFont(url) ;
    }
}

static void fetchFont(const char * url)
{
    emscripten_fetch_attr_t ttfAttr ;
    emscripten_fetch_attr_init(&ttfAttr) ;
    strcpy(ttfAttr.requestMethod, "GET") ;
    ttfAttr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY ;
    ttfAttr.onsuccess = ttfDownloadSucceeded ;
    ttfAttr.onerror = downloadFailed ;
    ttfAttr.onprogress = downloadProgress ;
    emscripten_fetch(&ttfAttr, url) ;
}

static void imgDownloadSucceeded(emscripten_fetch_t *fetch)
{
    static std::mutex mtx ;
  
    std::cout << "Finished downloading " << fetch->numBytes << " bytes from URL " <<  fetch->url << std::endl ;  
  
    std::string url {fetch->url} ;
    std::string fileName = url.substr(url.rfind('/') + 1) ;

    std::cout << fileName << std::endl ; 
  
    std::ofstream file (fileName.c_str(), std::ios::out|std::ios::binary) ;
    file.write (fetch->data,fetch->numBytes) ;
    file.close() ;
    SDL_Surface * image = IMG_Load(fileName.c_str()) ;

    {
        std::lock_guard<std::mutex> guard(mtx) ;
        Assets::images[fileName] = image ;
    }
  
    emscripten_fetch_close(fetch) ;
    loadedCount ++ ; 
}

static void ttfDownloadSucceeded(emscripten_fetch_t *fetch)
{
    static std::mutex mtx ;
  
    std::cout << "Finished downloading " << fetch->numBytes << " bytes from URL " <<  fetch->url << std::endl ;  

    std::string url {fetch->url} ;
    std::string fileName = url.substr(url.rfind('/')+1) ;

    std::cout << fileName << std::endl ; 
  
    std::ofstream file (fileName.c_str(), std::ios::out|std::ios::binary) ;
    file.write (fetch->data,fetch->numBytes) ;
    file.close() ;
    TTF_Font* font = TTF_OpenFont(fileName.c_str(), 50);

    {
        std::lock_guard<std::mutex> guard(mtx) ;
        Assets::fonts.insert({fileName, font}) ;
    }
  
    emscripten_fetch_close(fetch) ;
    loadedCount ++ ; 
}

static void downloadFailed(emscripten_fetch_t *fetch)
{
    std::cerr << "Downloading " << fetch->url << " failed, HTTP failure status code: " << fetch->status << std::endl ;
    emscripten_fetch_close(fetch) ; 
}

static void downloadProgress(emscripten_fetch_t *fetch) {
  if (fetch->totalBytes) {
    printf("Downloading %s.. %.2f%% complete.\n", fetch->url, fetch->dataOffset * 100.0 / fetch->totalBytes);
  } else {
    printf("Downloading %s.. %lld bytes complete.\n", fetch->url, fetch->dataOffset + fetch->numBytes);
  }
}

};
std::map<std::string, SDL_Surface *> Assets::images {} ;
std::map<std::string, TTF_Font *> Assets::fonts {} ;
std::vector<const char *> Assets::imgUrls 
{
//no images for this code
} ;
std::vector<const char *> Assets::ttfUrls 
{
    "https://raw.githubusercontent.com/google/fonts/main/ufl/ubuntumono/UbuntuMono-Regular.ttf",
};
std::atomic<int> Assets::loadedCount {0} ;








class EventListener
{
public:

EventListener (std::vector<SDL_EventType> etypes) : etypes{etypes}
{} 

bool correctType(SDL_Event event)
{
    auto it = std::find(etypes.begin(), etypes.end(), event.type);
    return (it != etypes.end()) ;
}

virtual bool consume (SDL_Event e) 
{
    return false ;
}

virtual ~EventListener()
{
    std::cout << "Deleting event listener" << std::endl ; 
}

protected:
std::vector<SDL_EventType> etypes;

} ;


class QuitOnEscape: public EventListener
{
public: 
QuitOnEscape() : EventListener({SDL_KEYDOWN})
{}

bool consume(SDL_Event event) override
{
    if(!correctType(event))
        return false ;
    if (event.key.keysym.sym == SDLK_ESCAPE)
    {
        emscripten_cancel_main_loop() ;
        return true ; 
    } 
    return false ;
}

} ;

class Widget 
{
public:
Widget(int x, int y, int width, int height):
rect{x,y,width,height}
{
  
}
virtual void render()
{
    
}
virtual ~Widget()
{
  
}
SDL_Rect rect ;
} ;

class Button : public Widget, public EventListener
{
public:
Button(SDL_Renderer * renderer, int x, int y, int width, int height, int border = 0) : 
Widget {x,y,width,height},
EventListener {{SDL_MOUSEBUTTONDOWN}},
renderer {renderer}
{
  SDL_Surface * surface = SDL_CreateRGBSurface(0, width, height, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff) ;
  SDL_Rect borderRect {0, 0, width, height} ;
  SDL_FillRect(surface, &borderRect, 0x000000ff) ;
  SDL_Rect innerRect {border, border, width - 2 * (border), height - 2 * (border)} ;
  SDL_FillRect(surface, &innerRect, 0x777777ff) ;
  texture = SDL_CreateTextureFromSurface(renderer, surface) ;
  SDL_FreeSurface(surface) ;

}
~Button()
{
  
}
void render () override
{
    SDL_RenderCopy(renderer, texture, 0, &rect);

    SDL_Color fg{0,0,0,255};
    SDL_Surface * textSurface = TTF_RenderUTF8_Solid(Assets::fonts["UbuntuMono-Regular.ttf"], "bųțțøñ", fg) ; //TTF_RenderUTF8_Solid(Assets::fonts["UbuntuMono-Regular.ttf"], "Webla Lisa", fg);
    SDL_Texture * textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_RenderCopy(renderer, textTexture, NULL, &rect) ;
    SDL_DestroyTexture(textTexture) ;
    SDL_FreeSurface(textSurface) ; 
}


bool consume(SDL_Event event) override
{
    if(!correctType(event))
        return false ;
    if(event.button.button == SDL_BUTTON_LEFT)
    {
        int x = event.button.x ;
        int y = event.button.y ;
        if( (x > rect.x) && (x < (rect.x + rect.w) ) )
        {
          if( (y > rect.y) && (y < (rect.y + rect.h) ) )
          {
            std::cout << "x:" << event.button.x 
                      <<" y:" << y << std::endl;
            EM_ASM(
              alert("!!click!!") ;
            ) ;
            return true ;
          }
        }
    }
    return false ;
}

SDL_Renderer * renderer ;
SDL_Texture * texture ;
};




// window configs { W = window width, H = window height }

#define W 360
#define H 180

//app singleton

class App
{
public:

static App& instance()
{
    static App INSTANCE {} ;
    return INSTANCE ;
}

void main()
{
    if(!Assets::fullyLoaded()) return ; 
    events() ;
    update() ;
    render() ;
}

private: 
void events()
{
    SDL_Event event {};
    while (SDL_PollEvent(&event)) 
    {
        auto it = eventListeners.begin() ; 
        while(it != eventListeners.end() && !((*it)->consume(event)))
        {
            it ++ ;
        }
    }
}

void update()
{
    
}

void render()
{
    SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255) ;
    SDL_RenderClear(renderer) ;
    for (auto & widget : widgets)
    {
        widget->render() ;
    }
    SDL_RenderPresent(renderer) ;
}

SDL_Window * window {} ;
SDL_Renderer * renderer {} ;
std::vector<EventListener *> eventListeners {} ;
std::vector<Widget *> widgets {} ;

App()
{
    SDL_Init(SDL_INIT_EVERYTHING) ;
    TTF_Init() ;
    SDL_CreateWindowAndRenderer(W, H, 0, &window, &renderer) ;

    Assets::init();
    Button * button = new Button(renderer, 130, 70, 100, 40, 1) ; 
    eventListeners.push_back(new QuitOnEscape()) ;
    eventListeners.push_back(button) ;
    widgets.push_back(button) ;
}

~App()
{
    std::cout << "Deleting app" << std::endl ;
    for (auto & eventListener: eventListeners) 
    {
        delete eventListener ;
        eventListener = nullptr ;
    }
    for (auto & widget: widgets) 
    {
        delete widget ;
        widget = nullptr ;
    }
    SDL_DestroyRenderer(renderer) ;
    SDL_DestroyWindow(window) ;
    SDL_Quit() ;
}

};







//program start / main

App &app = App::instance();

void main_loop()
{
    app.main();
}

int main(int argc, char** argv)
{
    
    emscripten_set_main_loop(&main_loop, -1, 1) ;
  
    return 0;
}


