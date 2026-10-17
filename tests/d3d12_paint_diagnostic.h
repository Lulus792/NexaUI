#ifndef NEXAUI_D3D12_PAINT_DIAGNOSTIC_H
#define NEXAUI_D3D12_PAINT_DIAGNOSTIC_H
/* Read-only investigation of the renderer: no shader or numerical contract
   changes. Coordinate-coded opaque sources separate texel choice from blend. */
static nx_command diagnostic_scaled(nx_command c,double scale) {
    c.rect.x=(float)(c.rect.x*scale);c.rect.y=(float)(c.rect.y*scale);
    c.rect.width=(float)(c.rect.width*scale);c.rect.height=(float)(c.rect.height*scale);
    c.radius=(float)(c.radius*scale);c.thickness=(float)(c.thickness*scale);
    for(unsigned i=0;i<3;++i){c.points[i].x=(float)(c.points[i].x*scale);c.points[i].y=(float)(c.points[i].y*scale);}
    return c;
}
static void diagnostic_difference(const char *name,const uint8_t *gpu,const uint8_t *ref,uint32_t w,uint32_t h) {
    size_t bytes=0,pixels=0,first=SIZE_MAX;unsigned maximum=0;
    for(size_t i=0;i<(size_t)w*h;++i){int different=0;
        for(unsigned c=0;c<4;++c){size_t at=4*i+c;unsigned d=gpu[at]>ref[at]?gpu[at]-ref[at]:ref[at]-gpu[at];
            if(d){++bytes;different=1;}if(d>maximum)maximum=d;}
        if(different){++pixels;if(first==SIZE_MAX)first=i;}
    }
    printf("%s: %zu pixels/%zu bytes, max %u",name,pixels,bytes,maximum);
    if(first!=SIZE_MAX)printf("; first (%zu,%zu) GPU %u,%u,%u,%u REF %u,%u,%u,%u",first%w,first/w,
        gpu[4*first],gpu[4*first+1],gpu[4*first+2],gpu[4*first+3],ref[4*first],ref[4*first+1],ref[4*first+2],ref[4*first+3]);
    puts("");
}
static int diagnostic_frame(nx_d3d12_renderer *renderer,const nx_command *commands,size_t count,
    double scale,nx_color clear,uint8_t *gpu,uint8_t *ref,uint32_t w,uint32_t h) {
    nx_command *scaled=malloc((count?count:1)*sizeof(*scaled));CHECK(scaled);
    for(size_t i=0;i<count;++i)scaled[i]=diagnostic_scaled(commands[i],scale);
    CHECK(nx_headless_render(scaled,count,ref,w,h,(size_t)w*4,clear)==NX_OK);free(scaled);
    CHECK(nx_d3d12_test_render_readback(renderer,commands,count,scale,clear,gpu,(size_t)w*4)==NX_OK);
    return 0;
}
static int diagnose_paints(nx_d3d12_renderer *renderer,uint32_t w,uint32_t h,uint8_t *gpu,uint8_t *ref) {
    const double scales[]={1,1.5,2};
    nx_d3d12_renderer_info info;CHECK(nx_d3d12_renderer_get_info(renderer,&info)==NX_OK);
    printf("D3D12 diagnostic: %ux%u, WARP %d\n",w,h,info.using_warp);
    for(unsigned density=0;density<3;++density){
        double scale=scales[density];nx_context *ui=NULL;nx_paint_image *images[3]={0};
        CHECK(nx_context_create(NULL,&ui)==NX_OK&&!paint_scene(ui,images,(float)scale));
        const nx_command *commands;size_t count;CHECK(nx_frame_commands(ui,&commands,&count)==NX_OK);
        printf("density/scale %.1f\n",scale);
        for(size_t i=0;i<count;++i){
            if(commands[i].type!=NX_COMMAND_BITMAP)continue;
            nx_command c=commands[i];char title[80];
            printf("bitmap %zu: %ux%u at %.9g,%.9g %.9g,%.9g, tint %.9g,%.9g,%.9g,%.9g\n",i,
                c.bitmap.width,c.bitmap.height,c.rect.x,c.rect.y,c.rect.width,c.rect.height,c.color.r,c.color.g,c.color.b,c.color.a);
            CHECK(!diagnostic_frame(renderer,&c,1,scale,(nx_color){.02f,.04f,.08f,1},gpu,ref,w,h));
            snprintf(title,sizeof(title),"original bitmap %zu",i);diagnostic_difference(title,gpu,ref,w,h);
            size_t source_bytes=(size_t)c.bitmap.width*c.bitmap.height*4;
            uint8_t *coordinates=malloc(source_bytes);CHECK(coordinates);
            for(uint32_t y=0;y<c.bitmap.height;++y)for(uint32_t x=0;x<c.bitmap.width;++x){
                uint8_t *p=coordinates+((size_t)y*c.bitmap.width+x)*4;
                p[0]=(uint8_t)x;p[1]=(uint8_t)y;p[2]=(uint8_t)((x>>8)|((y>>8)<<4));p[3]=255;
            }
            c.bitmap.pixels=coordinates;c.bitmap.stride_bytes=(size_t)c.bitmap.width*4;c.bitmap.format=NX_BITMAP_RGBA8;
            c.color=(nx_color){1,1,1,1};
            CHECK(!diagnostic_frame(renderer,&c,1,scale,(nx_color){0},gpu,ref,w,h));
            snprintf(title,sizeof(title),"coordinates bitmap %zu",i);diagnostic_difference(title,gpu,ref,w,h);
            free(coordinates);
        }
        CHECK(!diagnostic_frame(renderer,commands,count,scale,(nx_color){.02f,.04f,.08f,1},gpu,ref,w,h));
        diagnostic_difference("complete scene",gpu,ref,w,h);
        paint_scene_free(images);nx_context_destroy(ui);
    }
    return 0;
}
static int test_identity_texels(nx_d3d12_renderer *renderer,uint32_t w,uint32_t h,uint8_t *gpu,uint8_t *ref) {
    uint8_t pixels[49*73*4],mask[49*73];
    for(unsigned y=0;y<73;++y)for(unsigned x=0;x<49;++x){
        size_t p=((size_t)y*49+x)*4;pixels[p]=(uint8_t)x;pixels[p+1]=(uint8_t)y;pixels[p+2]=71;pixels[p+3]=255;
        mask[(size_t)y*49+x]=(uint8_t)(x+2*y);
    }
    const float origins[]={3.5f,nextafterf(-.5f,0),nextafterf(.5f,1),-.5f,.5f,9.25f};
    for(unsigned format=0;format<2;++format)for(unsigned axis=0;axis<3;++axis)for(unsigned i=0;i<sizeof(origins)/sizeof(*origins);++i){
        nx_command commands[3]={0};commands[0].type=NX_COMMAND_PUSH_CLIP;commands[0].rect=(nx_rect){4,4,32,32};
        commands[1].type=NX_COMMAND_BITMAP;commands[1].color=(nx_color){1,1,1,1};
        commands[1].bitmap=(nx_bitmap_view){pixels,49,73,49*4,NX_BITMAP_RGBA8};
        if(format)commands[1].bitmap=(nx_bitmap_view){mask,49,73,49,NX_BITMAP_A8};
        commands[1].rect=(nx_rect){axis==2?0:origins[i],axis==1?0:origins[i],axis==2?98:49,axis==1?146:73};
        commands[2].type=NX_COMMAND_POP_CLIP;
        CHECK(!diagnostic_frame(renderer,commands,3,1,(nx_color){0},gpu,ref,w,h));
        printf("identity format %u axes %u origin %.9g\n",format,axis,origins[i]);
        CHECK(compare_pixels(gpu,ref,w,h));
    }
    return 0;
}
#endif
