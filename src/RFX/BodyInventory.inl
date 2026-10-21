// Native select input retains BF2's ammo ownership, deploy checks and networking.
struct InventoryHandsState {
    void* owner=nullptr;void* identity=nullptr;void* pendingWeapon=nullptr;
    int selected=-1,pending=0;ULONGLONG requested=0;
    bool empty=false,dead=true,releaseArmed=false,physical=true,hold=true,glued=false;
} inventoryHands;
bool InventoryStowed(void* owner){
    // BF2 exposes the selected personal weapon in open passenger seats.
    // Preserve the on-foot holster state for exit, but do not use it to solve
    // empty wrists while leaving that visible seat weapon in its native pose.
    return owner&&inventoryHands.owner==owner&&inventoryHands.empty&&!inventoryHands.dead&&!PassengerWeaponSeat(owner);
}
struct BodySlotsState {
    void* owner=nullptr;void* identity=nullptr;std::uint64_t origin=0;
    ULONGLONG tick=0,scan=0;bool pressed=true,consumed=false,valid=false;
    float yaw=0;int hover=-1;int items[7]{};Mat anchors[7]{};
} bodySlots;
bool PhysicalReleaseCarry(){
    // Personal-weapon passenger seats auto-hold the selected item. Right
    // squeeze remains native Use/exit, so it cannot also gate carry/release.
    // Preserve the saved infantry preference and holster state for exit.
    return !inventoryHands.dead&&inventoryHands.physical&&!inventoryHands.glued&&!PassengerWeaponSeat(inventoryHands.owner);
}
void ClearInventoryCarryDrops();
void ResetInventoryMotion();
void SampleInventoryMotion(const Runtime::Frame& f,void* owner);
bool InventoryItemAway(int item,ULONGLONG now);
bool InventoryThrowBusy();
bool RightGrenadePrimed();
void ReleaseInventoryItem(const Runtime::Frame& f,void* owner,int item,int slot,bool holster,bool stationary=false);
void ResetBodySlots(){
    bodySlots.valid=false;bodySlots.tick=0;bodySlots.hover=-1;bodySlots.consumed=false;bodySlots.pressed=true;
    inventoryHands.pending=0;inventoryHands.pendingWeapon=nullptr;inventoryHands.releaseArmed=false;
    ResetInventoryMotion();
    if(!LocalAlive())inventoryHands.dead=true;
}
void* InventoryItem(void* owner,int index){
    if(!owner||index<1||index>9||Value<void*>(owner)!=game+0x4E80F8)return nullptr;
    if(Value<void*>(Value<void*>(owner,0x158),0x14)!=game+0x1A8C50)return nullptr;
    const auto begin=Value<std::uintptr_t>(owner,0x1FC),end=Value<std::uintptr_t>(owner,0x200);
    if(!begin||end<begin||end-begin>64||(end-begin)%4||index>=int((end-begin)/4))return nullptr;
    void* item=Value<void*>(reinterpret_cast<void*>(begin),index*4);
    // Inactive inventory entries are intentionally detached/disabled by BF2.
    // Exact vector membership is authoritative, not their render flags/parent.
    // Validate the native Object interface and template before reading a mesh.
    void* table=Value<void*>(item);char name[80]{};
    return item&&Value<void*>(table,24*4)==game+0x31D520&&
        Value<void*>(table,25*4)==game+0x31D530&&ReloadName(item,name)?item:nullptr;
}
bool HandGrenade(const char* name){return strstr(name,"hgr_")||strstr(name,"grenade")||!strcmp(name,"rgo");}
void ScanBodySlots(void* owner,BodySlotsState& slots){
    memset(slots.items,0,sizeof(slots.items));int gadget=0;
    for(int i=1;i<=9;++i){
        void* item=InventoryItem(owner,i);char name[80]{};if(!item||!ReloadName(item,name))continue;
        int slot=-1;
        if(i==1&&strstr(name,"knife"))slot=5;
        else if(i==2)slot=3;
        else if(i==3)slot=2;
        else if(HandGrenade(name))slot=4;
        else if(!strstr(name,"parachute")&&gadget<3){slot=gadget==2?6:gadget;++gadget;}
        if(slot>=0&&!slots.items[slot])slots.items[slot]=i;
    }
    static int last[7]{};static void* lastOwner=nullptr;
    if(lastOwner!=owner||memcmp(last,slots.items,sizeof(last))){
        if(lastOwner!=owner){
            const auto begin=Value<std::uintptr_t>(owner,0x1FC),end=Value<std::uintptr_t>(owner,0x200);
            if(begin&&end>=begin&&end-begin<=64)for(int i=1;i<int((end-begin)/4)&&i<=9;++i){
                void* item=Value<void*>(reinterpret_cast<void*>(begin),i*4);char name[80]{};ReloadName(item,name);
                Log("SLOTS51 entry=%d name=%s flags=%08X parent=%p mesh=%p accepted=%d",i,name,Value<unsigned>(item,4),Value<void*>(item,0x28),Value<void*>(item,0x38),InventoryItem(owner,i)!=nullptr);
            }
        }
        memcpy(last,slots.items,sizeof(last));lastOwner=owner;
        Log("SLOTS51 inventory owner=%p shoulder=%d,%d primary=%d pistol=%d grenade=%d knife=%d rearGadget=%d",owner,last[0],last[1],last[2],last[3],last[4],last[5],last[6]);
    }
}

BodySlotsState slotLayoutPreview;
int previewSlot=-1;
Vec SlotPoint(int i,float height,const float offsets[7][3]){
    const Vec defaults[]={{-.25f,-.20f,-.10f},{.25f,-.20f,-.10f},{.23f,-height,0},{-.23f,-height,0},{.18f,-.36f,.18f},{0,-height+.05f,.20f},{0,-.10f,-.28f}};
    return Add(defaults[i],{offsets[i][0],offsets[i][1],offsets[i][2]});
}
bool SlotPreviewCurrent(){
    return previewSlot>=0&&slotLayoutPreview.valid&&slotLayoutPreview.origin==pairFrame.origin&&
        pairFrame.tick>=slotLayoutPreview.tick&&pairFrame.tick-slotLayoutPreview.tick<=150;
}
void EndSlotPreview(){slotLayoutPreview={};previewSlot=-1;}
bool UpdateSlotPreview(const Runtime::Frame& f,const float offsets[7][3],float height,int selected){
    void* owner=nullptr;void* identity=nullptr;
    if(selected<0||selected>=7||!f.valid||!f.focused||!FootOwner(owner,identity)){
        EndSlotPreview();return false;
    }
    auto& s=slotLayoutPreview;
    if(!s.valid||s.owner!=owner||s.identity!=identity||s.origin!=f.origin){
        s={};s.owner=owner;s.identity=identity;s.origin=f.origin;
        // Keep this heading while editing: looking down or over a shoulder
        // must not rotate the selected slot away from the player's eyes.
        s.yaw=Heading(Level(f.head));
    }
    if(!s.scan||f.tick<s.scan||f.tick-s.scan>400){ScanBodySlots(owner,s);s.scan=f.tick;}
    Mat torso=Yaw(s.yaw);SetPosition(torso,Position(f.head));
    for(int i=0;i<7;++i){s.anchors[i]=torso;SetPosition(s.anchors[i],Point(SlotPoint(i,height,offsets),torso));}
    s.tick=f.tick;s.valid=true;previewSlot=selected;return true;
}

int CycleInventoryItem(void* owner,int equipped,int direction){
    // Native numbered selection immediately equips. Mouse-wheel selection can
    // wait for a fire-click confirmation, which an empty-hands gate consumes.
    if(direction!=1&&direction!=-1)return 0;
    for(int step=1;step<9;++step){
        const int item=((equipped-1+direction*step)%9+9)%9+1;
        void* weapon=InventoryItem(owner,item);char name[80]{};
        if(weapon&&ReloadName(weapon,name)&&!strstr(name,"parachute"))return item;
    }
    return 0;
}
int UpdateBodySlots(const Runtime::Frame& f,bool enabled,bool actions,bool grip,int cycle,bool& consume){
    consume=false;void* owner=nullptr;void* identity=nullptr;
    if(!enabled||!f.valid||!f.focused||!f.hands[1].gripValid||!FootOwner(owner,identity)){
        ResetBodySlots();return 0;
    }
    const auto tune=Tuning();auto& s=bodySlots;auto& hands=inventoryHands;const auto now=f.tick;
    const int equipped=Value<int>(owner,0x1E0);
    if(hands.owner!=owner||hands.identity!=identity||hands.dead){
        hands={};hands.owner=owner;hands.identity=identity;hands.selected=equipped;hands.dead=false;
        hands.physical=tune.bodySlots;hands.hold=tune.holdGrip;hands.glued=tune.gluedItems;
        hands.empty=tune.bodySlots&&(tune.spawnEmptyHands||(tune.holdGrip&&!tune.gluedItems&&!grip))&&InventoryGearSupported();
        s.tick=0;ResetInventoryMotion();
        Log("SLOTS51 new life owner=%p empty=%d physical=%d hold=%d",owner,hands.empty,tune.bodySlots,tune.holdGrip);
    }
    if(hands.physical!=tune.bodySlots||hands.hold!=tune.holdGrip||hands.glued!=tune.gluedItems){
        hands.physical=tune.bodySlots;hands.hold=tune.holdGrip;hands.glued=tune.gluedItems;
        hands.releaseArmed=false;hands.pending=0;hands.pendingWeapon=nullptr;s.tick=0;
        ClearInventoryCarryDrops();
        if(!tune.bodySlots)hands.empty=false;
        else if(tune.holdGrip&&!tune.gluedItems&&!grip)hands.empty=true;
        Log("SLOTS56 mode physical=%d glued=%d hold=%d empty=%d",hands.physical,hands.glued,hands.hold,hands.empty);
    }
    const bool needsGrip=tune.bodySlots&&tune.holdGrip&&!tune.gluedItems;
    const bool changed=s.owner!=owner||s.identity!=identity||s.origin!=f.origin||!s.tick||now<s.tick||now-s.tick>150;
    if(changed){
        s.owner=owner;s.identity=identity;s.origin=f.origin;s.yaw=Heading(Level(f.head));s.pressed=grip;s.scan=0;s.hover=-1;s.consumed=false;
        // Losing tracking/opening a menu is not a physical throw gesture.
        if(needsGrip&&!grip&&!hands.empty)hands.empty=true;
        hands.releaseArmed=!hands.empty&&grip;ResetInventoryMotion();
    }
    if(tune.bodySlots)SampleInventoryMotion(f,owner);
    if(hands.pending){
        if(InventoryItem(owner,hands.pending)!=hands.pendingWeapon||(needsGrip&&!grip)){
            hands.pending=0;hands.pendingWeapon=nullptr;
        }else if(equipped==hands.pending&&Value<void*>(hands.pendingWeapon,0x28)==owner){
            hands.empty=false;hands.selected=equipped;hands.pending=0;hands.pendingWeapon=nullptr;hands.releaseArmed=grip;
            Runtime::Haptic(1,.65f,85);Log("SLOTS51 draw confirmed nativeItem=%d",equipped);
        }else if(now-hands.requested>2000){
            Log("SLOTS51 draw timeout wanted=%d actual=%d",hands.pending,equipped);
            hands.pending=0;hands.pendingWeapon=nullptr;Runtime::Haptic(1,.2f,160);
        }
    }
    hands.selected=equipped;
    if(!tune.bodySlots){
        // Classic mode always shows the currently equipped native item. It
        // ignores physical grip/spawn-empty options and never drops on grip.
        hands.empty=false;hands.releaseArmed=false;s.pressed=grip;
        s.valid=false;s.hover=-1;s.tick=now;s.consumed=false;
        if(actions&&cycle&&!hands.pending&&!InventoryThrowBusy()){
            const int item=CycleInventoryItem(owner,equipped,cycle);
            if(item){
                hands.pending=item;hands.pendingWeapon=InventoryItem(owner,item);hands.requested=now;
                Log("SLOTS56 button select nativeItem=%d previous=%d",item,equipped);return item;
            }
        }
        return 0;
    }
    if(!s.scan||now-s.scan>400){ScanBodySlots(owner,s);s.scan=now;}
    const float dt=changed?0:std::min(.05f,float(now-s.tick)*.001f);
    const float error=Wrapped(Heading(Level(f.head))-s.yaw);
    if(std::abs(error)>.40f)s.yaw=Wrapped(s.yaw+(error-std::copysign(.40f,error))*(1-std::exp(-dt/.14f)));
    Mat torso=Yaw(s.yaw);SetPosition(torso,Position(f.head));
    const float height=tune.slotHeight;
    Vec points[7];for(int i=0;i<7;++i)points[i]=SlotPoint(i,height,tune.slotOffsets);
    int hover=-1,heldSlot=-1;float nearest=1.f;
    const Vec hand=Point(Position(f.hands[1].grip),Inverse(torso));
    for(int i=0;i<7;++i){
        s.anchors[i]=torso;SetPosition(s.anchors[i],Point(points[i],torso));
        if(s.items[i]==equipped)heldSlot=i;
        if(!s.items[i]||InventoryItemAway(s.items[i],now))continue;
        const Vec d=Sub(hand,points[i]);float distance=Length(d)/tune.slotRadius;
        if(i<2){
            // Broad side-specific shoulder volumes extend above and forward.
            if(hand.y<points[i].y-.23f||(i==0?hand.x>points[i].x+.19f:hand.x<points[i].x-.19f))continue;
            distance=Length({d.x/.27f,d.y/.34f,d.z/.34f});
        }else if(i==6){
            // Centered behind the head, distinct from the two side shoulders.
            if(hand.z>points[i].z+.13f)continue;
            distance=Length({d.x/.20f,d.y/.28f,d.z/.23f});
        }
        if(distance<nearest){nearest=distance;hover=i;}
    }
    if(actions&&hover!=s.hover&&hover>=0&&!grip&&tune.bodySlots)Runtime::Haptic(1,.15f,30);
    s.hover=hover;s.valid=true;s.tick=now;int selected=0;
    // Utility clicks suppress new grab actions, not inventory tracking or
    // visibility. Keep pending draws, motion samples and slot anchors alive.
    // A real grip release still drops a held item in hold-grip mode.
    const bool pressed=actions&&grip&&!s.pressed,released=!grip&&s.pressed;
    if(!grip)s.consumed=false;
    if(tune.gluedItems&&pressed&&hover>=0)s.consumed=true;
    if(RightGrenadePrimed()&&!hands.empty&&tune.gluedItems&&pressed&&!InventoryThrowBusy()){
        // An armed frag cannot be silently exchanged into a holster.
        ReleaseInventoryItem(f,owner,equipped,heldSlot,false);
        hands.empty=true;hands.releaseArmed=false;s.consumed=true;selected=-1;
    }else if(!InventoryThrowBusy()&&!hands.pending){
        if(tune.gluedItems&&!hands.empty&&pressed&&hover>=0&&s.items[hover]!=equipped){
            // Touch another occupied slot and squeeze to exchange directly.
            // Keep the old item visible until native selection acknowledges;
            // the old item then appears at its own slot without a drop.
            const int item=s.items[hover];void* weapon=InventoryItem(owner,item);
            if(weapon&&!InventoryItemAway(item,now)){
                hands.pending=item;hands.pendingWeapon=weapon;hands.requested=now;
                hands.releaseArmed=false;s.consumed=true;selected=item;
                Log("SLOTS56 glued swap requested bodySlot=%d nativeItem=%d previous=%d",hover,item,equipped);
            }
        }else if(!hands.empty&&!tune.gluedItems&&((tune.holdGrip&&released&&hands.releaseArmed)||(!tune.holdGrip&&pressed))){
            ReleaseInventoryItem(f,owner,equipped,heldSlot,tune.bodySlots&&hover==heldSlot);
            hands.empty=true;hands.releaseArmed=false;s.consumed=true;selected=-1;
        }else if(hands.empty&&pressed){
            const int item=tune.bodySlots?(hover>=0?s.items[hover]:0):equipped;
            void* weapon=InventoryItem(owner,item);
            if(weapon&&!InventoryItemAway(item,now)){
                s.consumed=true;
                if(equipped==item&&Value<void*>(weapon,0x28)==owner){
                    hands.empty=false;hands.releaseArmed=true;selected=-1;Runtime::Haptic(1,.65f,85);
                    Log("SLOTS51 draw confirmed bodySlot=%d nativeItem=%d alreadySelected=1",hover,item);
                }else{
                    hands.pending=item;hands.pendingWeapon=weapon;hands.requested=now;selected=item;
                    Log("SLOTS51 draw requested bodySlot=%d nativeItem=%d previous=%d",hover,item,equipped);
                }
            }
        }
    }
    if(!hands.empty&&grip)hands.releaseArmed=true;
    s.pressed=grip;consume=s.consumed||(!hands.empty&&grip&&!tune.gluedItems)||hands.pending||InventoryThrowBusy();return selected;
}
