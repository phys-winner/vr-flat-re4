// Fit the authored button plane to the phone view; the native root transform moves visuals and hit areas together.
#ifndef RE4FIX_UI_FRAME_H
#define RE4FIX_UI_FRAME_H
struct fv_plane { struct reload_tf transform; float width, height; };
// A mesh's local up axis need not match its text. Keep its rectangle but use an owned control's upright basis.
static int fv_plane_basis(const struct fv_plane* plane,qf_t basis,struct fv_plane* result) {
    if(!plane || !result || !reload_marker_valid(&plane->transform) || !isfinite(plane->width) || !isfinite(plane->height)
        || plane->width<=0 || plane->height<=0)return 0;
    float norm=basis.x*basis.x+basis.y*basis.y+basis.z*basis.z+basis.w*basis.w;
    if(!isfinite(norm) || fabsf(norm-1)>.01f)return 0;
    norm=sqrtf(norm);basis.x/=norm;basis.y/=norm;basis.z/=norm;basis.w/=norm;
    qf_t inverse={-basis.x,-basis.y,-basis.z,basis.w};
    float lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
    for(unsigned corner=0;corner<4;corner++) {
        float local[3]={0,(corner&1 ? .5f:-.5f)*plane->width*plane->transform.s[1],
            (corner&2 ? .5f:-.5f)*plane->height*plane->transform.s[2]},world[3],aligned[3];
        q_rot(plane->transform.q,local,world);q_rot(inverse,world,aligned);
        for(unsigned k=0;k<3;k++){lo[k]=fminf(lo[k],aligned[k]);hi[k]=fmaxf(hi[k],aligned[k]);}
    }
    float width=hi[1]-lo[1],height=hi[2]-lo[2];
    if(width<1 || height<1 || width>16384 || height>16384 || hi[0]-lo[0]>fmaxf(width,height)*.01f)return 0;
    *result=*plane;result->transform.q=basis;result->transform.s[0]=result->transform.s[1]=result->transform.s[2]=1;
    float centre[3],shift[3];for(unsigned k=0;k<3;k++)centre[k]=(hi[k]+lo[k])*.5f;
    q_rot(basis,centre,shift);for(unsigned k=0;k<3;k++)result->transform.p[k]+=shift[k];
    result->width=width;result->height=height;
    return reload_marker_valid(&result->transform);
}
// Convert a flat authored mesh bound to the UI plane basis (+X normal, Y width, Z height).
static int fv_mesh_plane(const struct reload_tf* transform,const float origin[3],const float extent[3],struct fv_plane* plane) {
    if (!transform || !origin || !extent || !plane || !reload_marker_valid(transform)) return 0;
    unsigned normal=0;
    for(unsigned i=0;i<3;i++) {
        if(!isfinite(origin[i]) || !isfinite(extent[i]) || extent[i]<0 || extent[i]>100000)return 0;
        if(extent[i]<extent[normal])normal=i;
    }
    unsigned width=normal==1 ? 0:1,height=normal==2 ? 0:2;
    if(extent[width]<.5f || extent[height]<.5f || extent[normal]>fminf(extent[width],extent[height])*.01f)return 0;
    plane->transform=*transform;
    qf_t basis=normal==1 ? (qf_t){0,0,.7071067811865475f,.7071067811865475f}
        : normal==2 ? (qf_t){0,-.7071067811865475f,0,.7071067811865475f} : (qf_t){0,0,0,1};
    plane->transform.q=q_mul(transform->q,basis);
    float norm=sqrtf(plane->transform.q.x*plane->transform.q.x+plane->transform.q.y*plane->transform.q.y
        +plane->transform.q.z*plane->transform.q.z+plane->transform.q.w*plane->transform.q.w);
    plane->transform.q.x/=norm;plane->transform.q.y/=norm;plane->transform.q.z/=norm;plane->transform.q.w/=norm;
    float local[3],world[3];for(unsigned i=0;i<3;i++)local[i]=origin[i]*transform->s[i];
    q_rot(transform->q,local,world);
    for(unsigned i=0;i<3;i++)plane->transform.p[i]+=world[i];
    plane->transform.s[0]=transform->s[normal];plane->transform.s[1]=transform->s[width];plane->transform.s[2]=transform->s[height];
    plane->width=extent[width]*2;plane->height=extent[height]*2;
    return reload_marker_valid(&plane->transform);
}
static int fv_world_popup_type(unsigned type) { return type==1 || type==7; }
static int fv_front_priorities(const struct reload_tf* camera,const struct reload_tf* widgets,unsigned count,int* priorities) {
    if (!camera || !widgets || !priorities || !count || count>64 || !reload_tf_valid(camera)) return 0;
    qf_t inverse={-camera->q.x,-camera->q.y,-camera->q.z,camera->q.w};
    float depth[64];
    for (unsigned i=0;i<count;i++) {
        if (!reload_marker_valid(widgets+i)) return 0;
        float delta[3],view[3];for(unsigned k=0;k<3;k++)delta[k]=widgets[i].p[k]-camera->p[k];
        q_rot(inverse,delta,view);depth[i]=view[0];
        if (!isfinite(depth[i]) || depth[i]<=0 || depth[i]>2000) return 0;
    }
    // No-depth widgets must sort by their planes: a lower button's centre can be farther than its background.
    for (unsigned i=0;i<count;i++) {
        priorities[i]=1;
        for (unsigned j=0;j<count;j++) if (depth[j]>depth[i]+.001f) priorities[i]++;
    }
    return 1;
}
static int fv_footer_visual_depth(const struct reload_tf* content,const struct fv_plane* row,
                                  const struct fv_plane* button,const struct fv_plane* row_visual,
                                  const struct fv_plane* button_visual,struct reload_tf* relative) {
    if (!reload_marker_valid(content) || !reload_marker_valid(&row->transform)
        || !reload_marker_valid(&button->transform) || !reload_marker_valid(&row_visual->transform)
        || !reload_marker_valid(&button_visual->transform) || !reload_marker_valid(relative)
        || row->width<=0 || row->height<=0 || button->width<=0 || button->height<=0) return 0;
    float scale=fminf(row->width*row->transform.s[1]/(button->width*button->transform.s[1]),
                      row->height*row->transform.s[2]/(button->height*button->transform.s[2]));
    qf_t inv_button={-button->transform.q.x,-button->transform.q.y,-button->transform.q.z,button->transform.q.w};
    qf_t inv_content={-content->q.x,-content->q.y,-content->q.z,content->q.w};
    qf_t correction=q_mul(row->transform.q,inv_button);
    float delta[3],moved[3],shift[3],local[3];
    for(unsigned k=0;k<3;k++)delta[k]=(button_visual->transform.p[k]-button->transform.p[k])*scale;
    q_rot(correction,delta,moved);
    for(unsigned k=0;k<3;k++)shift[k]=row_visual->transform.p[k]-row->transform.p[k]-moved[k];
    q_rot(inv_content,shift,local);
    if (!isfinite(local[0]) || fabsf(local[0])>10) return 0;
    // Footer widgets sit behind their hit plane; match the actual row quad so the console cannot cover them.
    relative->p[0]+=local[0]/content->s[0];
    return reload_marker_valid(relative);
}
// Match the first footer button to the first authored list row, then append after all actual rows.
static int fv_footer_transform(const struct reload_tf* content, const struct fv_plane* row,
                               const struct reload_tf* root, const struct fv_plane* button,
                               unsigned rows, float spacing, struct reload_tf* relative) {
    if (!content || !row || !root || !button || !relative || !rows || rows > 128
        || !reload_marker_valid(content) || !reload_marker_valid(root)
        || !reload_marker_valid(&row->transform) || !reload_marker_valid(&button->transform)
        || !isfinite(spacing) || spacing < 1 || spacing > 100
        || !isfinite(row->width) || !isfinite(button->width) || row->width <= 0 || button->width <= 0
        || !isfinite(row->height) || !isfinite(button->height) || row->height <= 0 || button->height <= 0) return 0;
    float scale = fminf(row->width * row->transform.s[1] / (button->width * button->transform.s[1]),
                        row->height * row->transform.s[2] / (button->height * button->transform.s[2]));
    if (!isfinite(scale) || scale < .1f || scale > 4) return 0;
    qf_t inv_content = {-content->q.x,-content->q.y,-content->q.z,content->q.w};
    qf_t inv_button = {-button->transform.q.x,-button->transform.q.y,-button->transform.q.z,button->transform.q.w};
    qf_t correction = q_mul(row->transform.q, inv_button);
    qf_t desired = q_mul(correction, root->q);
    *relative = *root;
    relative->q = q_mul(inv_content, desired);
    float offset[3], moved[3], tail[3], delta[3], local[3] = {0,0,-(float)rows*spacing*content->s[2]};
    for (unsigned k=0;k<3;k++) offset[k]=(button->transform.p[k]-root->p[k])*scale;
    q_rot(correction,offset,moved);q_rot(content->q,local,tail);
    for (unsigned k=0;k<3;k++) delta[k]=row->transform.p[k]+tail[k]-moved[k]-content->p[k];
    q_rot(inv_content,delta,relative->p);
    for (unsigned k=0;k<3;k++) {relative->p[k]/=content->s[k];relative->s[k]=root->s[k]*scale/content->s[k];}
    return reload_marker_valid(relative);
}
static float fv_console_fov(float authored) {
    if (!isfinite(authored) || authored < 30 || authored > 130) return authored;
    // The authored console is fixed: rows, hover states and scrolling must never change the camera.
    return 2 * atanf(tanf(authored * .00872664625997165f) / 1.5f) * 57.2957795130823f;
}
static int fv_frame_fit(const struct reload_tf* camera, const struct reload_tf* root,
                              const struct fv_plane* planes, unsigned count, float aspect, float fov,
                              struct reload_tf* result) {
    if (!camera || !root || !planes || !result || !count || count > 64 || !reload_tf_valid(camera)
        // A bounded phone subregion may be much wider than the complete viewport (up to 4/.05 =80).
        || !reload_marker_valid(root) || !isfinite(aspect) || aspect < .01f || aspect > 80
        || !isfinite(fov) || fov < 1 || fov > 130) return 0;
    qf_t basis = planes[0].transform.q, inverse = {-basis.x, -basis.y, -basis.z, basis.w};
    float lo[3] = {INFINITY, INFINITY, INFINITY}, hi[3] = {-INFINITY, -INFINITY, -INFINITY};
    for (unsigned i = 0; i < count; i++) {
        const struct fv_plane* plane = planes + i;
        if (!reload_marker_valid(&plane->transform) || !isfinite(plane->width) || !isfinite(plane->height)
            || plane->width <= 0 || plane->width > 16384 || plane->height <= 0 || plane->height > 16384) return 0;
        for (unsigned corner = 0; corner < 4; corner++) {
            float local[3] = {0, ((corner & 1) ? .5f : -.5f) * plane->width * plane->transform.s[1],
                                 ((corner & 2) ? .5f : -.5f) * plane->height * plane->transform.s[2]}, world[3], aligned[3];
            q_rot(plane->transform.q, local, world);
            for (unsigned k = 0; k < 3; k++) world[k] += plane->transform.p[k] - planes[0].transform.p[k];
            q_rot(inverse, world, aligned);
            for (unsigned k = 0; k < 3; k++) { lo[k] = fminf(lo[k], aligned[k]); hi[k] = fmaxf(hi[k], aligned[k]); }
        }
    }
    float width = hi[1] - lo[1], height = hi[2] - lo[2];
    if (!isfinite(width) || !isfinite(height) || width < 1 || height < 1
        || width > 4000 || height > 4000 || hi[0] - lo[0] > fmaxf(width, height) * .1f) return 0;
    const float distance = 100.0f;
    float half_width = distance * tanf(fov * .00872664625997165f);
    float scale = fminf(half_width * 1.72f / width, half_width * 1.72f / (aspect * height));
    if (!isfinite(scale) || scale < .001f || scale > 1000) return 0;
    float normal[3], axis[3] = {1, 0, 0}; q_rot(basis, axis, normal);
    float facing = 0;
    for (unsigned k = 0; k < 3; k++) facing += normal[k] * (camera->p[k] - planes[0].transform.p[k]);
    qf_t desired = camera->q;
    if (facing > 0) desired = q_mul(desired, (qf_t){0, 0, 1, 0});
    qf_t correction = q_mul(desired, inverse);
    *result = *root;
    result->q = q_mul(correction, root->q);
    float norm=sqrtf(result->q.x*result->q.x+result->q.y*result->q.y+result->q.z*result->q.z+result->q.w*result->q.w);
    result->q.x/=norm;result->q.y/=norm;result->q.z/=norm;result->q.w/=norm;
    float centre_local[3] = {(hi[0] + lo[0]) * .5f, (hi[1] + lo[1]) * .5f, (hi[2] + lo[2]) * .5f};
    float centre_world[3], shifted[3], forward[3];
    q_rot(basis, centre_local, centre_world);
    for (unsigned k = 0; k < 3; k++) centre_world[k] = (centre_world[k] + planes[0].transform.p[k] - root->p[k]) * scale;
    q_rot(correction, centre_world, shifted);
    q_rot(camera->q, axis, forward);
    for (unsigned k = 0; k < 3; k++) {
        result->p[k] = camera->p[k] + forward[k] * distance - shifted[k];
        result->s[k] *= scale;
        if (!isfinite(result->p[k]) || fabsf(result->p[k]) > 10000000
            || !isfinite(result->s[k]) || result->s[k] < .001f || result->s[k] > 1000) return 0;
    }
    return reload_marker_valid(result);
}
static int fv_frame_transform(const struct reload_tf* camera,const struct reload_tf* root,
                              const struct fv_plane* planes,unsigned count,float aspect,float fov,
                              struct reload_tf* result) {
    if (!isfinite(aspect) || aspect<1 || aspect>4 || !isfinite(fov) || fov<30 || fov>130) return 0;
    return fv_frame_fit(camera,root,planes,count,aspect,fov,result);
}
// A menu can spawn its buttons as independent actors. Frame its authored surface from the same eye
// instead of moving only one root: visuals, original hit planes and native transitions remain together.
static int fv_bounds_camera(const struct reload_tf* camera,const struct reload_tf* surface,
                            const float origin[3],const float extent[3],float aspect,
                            struct reload_tf* desired,float* fov) {
    if(!camera || !surface || !origin || !extent || !desired || !fov || !reload_tf_valid(camera)
        || !reload_marker_valid(surface) || !isfinite(aspect) || aspect<1 || aspect>4)return 0;
    float corners[8][3],centre[3]={0};
    for(unsigned k=0;k<3;k++)if(!isfinite(origin[k]) || !isfinite(extent[k]) || extent[k]<0
        || fabsf(origin[k])>100000 || extent[k]>100000)return 0;
    for(unsigned c=0;c<8;c++) {
        float local[3],world[3];
        for(unsigned k=0;k<3;k++)local[k]=(origin[k]+(c&(1u<<k) ? extent[k]:-extent[k]))*surface->s[k];
        q_rot(surface->q,local,world);
        for(unsigned k=0;k<3;k++){corners[c][k]=surface->p[k]+world[k]-camera->p[k];centre[k]+=corners[c][k]*.125f;}
    }
    float horizontal=hypotf(centre[0],centre[1]),distance=hypotf(horizontal,centre[2]);
    if(!isfinite(distance) || distance<1 || horizontal<.001f)return 0;
    float yaw=atan2f(centre[1],centre[0]),pitch=atan2f(centre[2],horizontal);
    float forward[3]={centre[0]/distance,centre[1]/distance,centre[2]/distance};
    float right[3]={-sinf(yaw),cosf(yaw),0},up[3]={-sinf(pitch)*cosf(yaw),-sinf(pitch)*sinf(yaw),cosf(pitch)};
    float tangent=0;
    for(unsigned c=0;c<8;c++) {
        float depth=0,x=0,y=0;
        for(unsigned k=0;k<3;k++){depth+=corners[c][k]*forward[k];x+=corners[c][k]*right[k];y+=corners[c][k]*up[k];}
        if(!isfinite(depth) || depth<=1)return 0;
        tangent=fmaxf(tangent,fmaxf(fabsf(x)/depth,fabsf(y)*aspect/depth));
    }
    float fitted=2*atanf(tangent/.86f)*57.2957795130823f;
    if(!isfinite(fitted) || fitted<1 || fitted>130)return 0;
    *desired=*camera;
    desired->q=q_mul((qf_t){0,0,sinf(yaw*.5f),cosf(yaw*.5f)},(qf_t){0,-sinf(pitch*.5f),0,cosf(pitch*.5f)});
    *fov=fmaxf(30,fitted);return reload_tf_valid(desired);
}
// Remove the current OVR head pose from the native camera before deriving the next pose. This avoids
// accumulating the same correction every frame when the MenuPawn anchor inherited the gameplay heading.
static int fv_camera_head(const struct reload_tf* camera,const struct reload_tf* desired,
                          float current_yaw,float current_pitch,float* yaw,float* pitch) {
    if(!camera || !desired || !yaw || !pitch || !reload_tf_valid(camera) || !reload_tf_valid(desired)
        || !isfinite(current_yaw) || !isfinite(current_pitch))return 0;
    qf_t head=q_mul((qf_t){0,0,-sinf(current_yaw*.5f),cosf(current_yaw*.5f)},
                   (qf_t){0,-sinf(current_pitch*.5f),0,cosf(current_pitch*.5f)});
    qf_t base=q_mul(camera->q,(qf_t){-head.x,-head.y,-head.z,head.w});
    qf_t relative=q_mul((qf_t){-base.x,-base.y,-base.z,base.w},desired->q);
    float forward[3];q_rot(relative,(float[3]){1,0,0},forward);
    float y=-atan2f(forward[1],forward[0]),p=atan2f(forward[2],hypotf(forward[0],forward[1]));
    qf_t check=q_mul((qf_t){0,0,-sinf(y*.5f),cosf(y*.5f)},(qf_t){0,-sinf(p*.5f),0,cosf(p*.5f)});
    float dot=fabsf(relative.x*check.x+relative.y*check.y+relative.z*check.z+relative.w*check.w);
    if(!isfinite(dot) || dot<.9999f || fabsf(p)>1.553343034f)return 0;
    *yaw=y;*pitch=p;return 1;
}
struct fv_region {float x,y,width,height;};
static int fv_frame_region(const struct reload_tf* camera,const struct reload_tf* root,
                           const struct fv_plane* planes,unsigned count,float aspect,float fov,
                           const struct fv_region* region,struct reload_tf* result) {
    if (!region || !isfinite(aspect) || aspect<1 || aspect>4 || !isfinite(fov) || fov<30 || fov>130
        || !isfinite(region->x) || !isfinite(region->y) || !isfinite(region->width) || !isfinite(region->height)
        || region->x<0 || region->y<0 || region->width<.05f || region->height<.05f
        || region->x+region->width>1 || region->y+region->height>1) return 0;
    float tangent=tanf(fov*.00872664625997165f);
    float virtual_fov=2*atanf(tangent*region->width)*57.2957795130823f;
    if (!fv_frame_fit(camera,root,planes,count,aspect*region->width/region->height,virtual_fov,result)) return 0;
    float shift[3],local[3]={0,(2*(region->x+region->width*.5f)-1)*100*tangent,
                              (1-2*(region->y+region->height*.5f))*100*tangent/aspect};
    q_rot(camera->q,local,shift);
    for (unsigned k=0;k<3;k++) {
        result->p[k]+=shift[k];
        if (!isfinite(result->p[k]) || fabsf(result->p[k])>10000000) return 0;
    }
    return reload_marker_valid(result);
}
#endif
