/*
  MGM_LedRing_Stand60.scad
  ---------------------------------------------------------------
  ESP32-C3 0.42" OLED kart + 12'li WS2812 halka icin minimum kutu
  - Altta ESP32-C3, ustte LED halka, en ustte difuzor kapak
  - USB-C girisi yanda (kablo kafasi icin tunel)
  - Kapakta OLED penceresi + BOOT/RST icin esnek basma tuslari

  Parcalar:  part = "base" | "lid" | "assembly"
  Baski:
    Govde : PLA/PETG, opak, destek GEREKMEZ (tunel ustu kopru)
    Kapak : BEYAZ/seffaf PLA veya PETG, ust yuzey tablaya (dosyada ters cevrili)

  Kart olculeri ESP32-C3-OLED4.2.step dosyasindan alindi.
  Halka: 50/35 mm WS2812 12'li (urun bilgisi).
  LEHIM PAYI: halka altinda tam bosluk, sadece 3 kucuk destek ayagi var;
  halkayi lehim pedleri ayaklara denk gelmeyecek sekilde cevir.
  MGM - 2026
  ---------------------------------------------------------------
*/

part = "assembly";

$fn = 128;
eps = 0.01;

// ---------- LED halka ----------
ring_od   = 50.0;   // dis cap
ring_id   = 35.0;   // ic cap (urun bilgisi: 50 / 35 mm)
ring_t    = 1.6;    // PCB kalinligi
led_h     = 1.6;    // 5050 LED yuksekligi
led_gap   = 1.5;    // LED ile kapak arasi (difuzyon)

// ---------- ESP32-C3 0.42" OLED kart ----------
pcb_l     = 24.95;   // USB yonundeki uzunluk (X)
pcb_w     = 20.7;   // genislik (Y)
pcb_t     = 1.15;
under     = 2.5;    // kart altinda kablo/lehim boslugu
usb_w     = 9.2;
usb_h     = 3.26;
usb_over  = 1.8;    // USB-C'nin PCB kenarindan tasmasi
board_x   = -2.5;    // kartin merkezden USB tarafina kaydirilmasi (tunel kisalir)

// Kart uzerindeki konumlar (kart merkezine gore, +X = USB tarafi)
oled_pos  = [-2.275, 0];  // OLED aktif alan merkezi (STEP)
oled_size = [6.7, 10.5];  // aktif alan 5.7 x 9.5 + 0.5 pay
btn1_pos  = [-10.53,  6.45];  // tus 1 (STEP)
btn2_pos  = [-10.53, -6.45];  // tus 2 (STEP)
btn_h     = 2.01;          // tusun PCB ustunden yuksekligi (STEP)

// ---------- Kutu ----------
wall      = 1.8;
floor_t   = 1.2;
lid_t     = 1.0;
clr       = 0.3;
plug_w    = 12.5;   // USB-C kablo kafasi (overmold) icin tunel
plug_h    = 7.0;
plug_r    = 2.0;

// ---------- Turetilen degerler ----------
r_pocket  = ring_od/2 + 0.5;          // halka yuvasi
outer_d   = 59.4;   // Base-Tilted orta bosluk 60 mm - 0.3 bosluk
R_out     = outer_d/2;
r_cav     = r_pocket;                 // halka alti tamamen bos (lehim/kablo payi)
sup_ang   = [60, 180, 300];           // halka destek ayaklari (USB tuneli 0 derecede)
sup_w     = 4.0;                      // destek genisligi (cevresel)
sup_in    = ring_od/2 - 2.0;          // destek ic yaricapi (2 mm temas)

pcb_bot   = floor_t + under;
pcb_top   = pcb_bot + pcb_t;
usb_cz    = pcb_top + usb_h/2;
tun_bot   = usb_cz - plug_h/2;
tun_top   = usb_cz + plug_h/2;
ring_bot  = tun_top + 0.8;
ring_top  = ring_bot + ring_t;
led_top   = ring_top + led_h;
lid_bot   = led_top + led_gap;
H_total   = lid_bot + lid_t;

usb_face  = board_x + pcb_l/2 + usb_over;

echo(str("Dis cap: ", 2*R_out, " mm  Yukseklik: ", H_total, " mm"));
echo(str("USB tunel uzunlugu: ", R_out - usb_face, " mm"));

// =================================================================
module rrect(size, r) {   // XY'de yuvarlak koseli dikdortgen, Z boyunca
  hull() for (x=[r, size[0]-r], y=[r, size[1]-r])
    translate([x, y, 0]) cylinder(r=r, h=size[2]);
}

module board_footprint(h, extra=0) {
  translate([board_x - pcb_l/2 - clr - extra, -pcb_w/2 - clr - extra, 0])
    cube([pcb_l + 2*clr + 2*extra, pcb_w + 2*clr + 2*extra, h]);
}

module corner_guides() {
  g = 1.2;    // kalinlik
  L = 3.0;    // kol uzunlugu
  for (sx=[-1,1], sy=[-1,1]) {
    cx = board_x + sx*(pcb_l/2 + clr);
    cy = sy*(pcb_w/2 + clr);
    difference() {
      translate([cx - (sx>0 ? L : g), cy - (sy>0 ? L : g), floor_t - eps])
        cube([L + g, L + g, pcb_top + 0.8 - floor_t]);
      // kartin oturdugu yer (kose altinda raf kalir)
      translate([0, 0, pcb_bot]) board_footprint(10);
      // raf sadece en kosede 1 mm (lehim pedlerine degmesin)
      translate([0, 0, floor_t]) board_footprint(10, sx > 0 ? 0 : -clr - 1.0);   // USB ucunda raf yok (5V/GND pedleri)
    }
  }
}

module usb_support() {   // USB ucunda, pedlerden uzak orta destek
  translate([board_x + pcb_l/2 - 0.95, -2, floor_t - eps]) cube([0.9, 4, pcb_bot - floor_t + eps]);
}

module ring_supports() {
  for (a=sup_ang) rotate([0,0,a])
    intersection() {
      difference() {
        cylinder(r=r_pocket + 0.1, h=ring_bot);
        translate([0,0,-1]) cylinder(r=sup_in, h=ring_bot + 2);
      }
      translate([0, -sup_w/2, 0]) cube([R_out, sup_w, ring_bot]);
    }
}

module base() {
  difference() {
    cylinder(r=R_out, h=lid_bot);
    // halka altindaki bosluk
    translate([0,0,floor_t]) cylinder(r=r_cav, h=lid_bot);
    // halka yuvasi
    translate([0,0,ring_bot]) cylinder(r=r_pocket, h=lid_bot);
    // USB-C tuneli
    translate([usb_face - 1, -plug_w/2, tun_bot])
      rotate([0,0,0]) translate([0,0,0])
        multmatrix([[0,0,1,0],[0,1,0,0],[1,0,0,0]])   // rrect'i X yonune cevir
          rrect([plug_h, plug_w, R_out - usb_face + 3], plug_r);
    // 3 mm kablo kanaliyla ayni hizada kablo cikisi (tunelin devami)
    // alt yuzey yazisi
    translate([0, 0, -eps]) linear_extrude(0.4)
      mirror([1,0,0]) text("MGM", size=7, halign="center", valign="center",
                           font="Liberation Sans:style=Bold");
  }
  corner_guides();
  usb_support();
  ring_supports();
}

module tongue_cut(p) {
  // menteşe -X tarafinda, uc tusun biraz ilerisinde
  len = 9.0; w = 5.0; gap = 0.6;
  x0 = board_x + p[0] - len + 1.2;
  y0 = p[1] - w/2;
  difference() {
    translate([x0, y0 - gap, -1]) cube([len + gap, w + 2*gap, lid_t + 2]);
    translate([x0 - eps, y0, -2]) cube([len, w, lid_t + 4]);
  }
}

pin_d    = 3.0;
pin_col  = 4.6;   // kapak altinda kalan yaka (pimin disari dusmesini onler)
pin_up   = 1.0;   // kapaktan disari tasma

module pin() {    // montaj konumunda (tus ustunde)
  zb = pcb_top + btn_h + 0.2;
  translate([0,0,zb]) cylinder(d=pin_d, h=lid_bot + lid_t + pin_up - zb, $fn=40);
  translate([0,0,lid_bot - 1.0 - 0.2]) cylinder(d=pin_col, h=1.0, $fn=40);
}

module lid() {   // montaj konumunda
  plunger_bot = pcb_top + btn_h + 0.3;
  difference() {
    union() {
      translate([0,0,lid_bot]) cylinder(r=R_out, h=lid_t);
      // dis dudak: govdeye gecer
      translate([0,0,lid_bot - 1.2]) difference() {
        cylinder(r=r_pocket - 0.2, h=1.2 + eps);
        translate([0,0,-1]) cylinder(r=r_pocket - 0.9, h=4);
      }
      // ic boru: halkayi bastirir, ortaya isik sizmasini engeller
      translate([0,0,ring_top]) difference() {
        cylinder(r=ring_id/2 + 1.0, h=lid_bot - ring_top + eps);
        translate([0,0,-1]) cylinder(r=ring_id/2 - 0.2, h=20);
      }
    }
    // OLED penceresi
    translate([board_x + oled_pos[0] - oled_size[0]/2,
               oled_pos[1] - oled_size[1]/2, lid_bot - 1])
      cube([oled_size[0], oled_size[1], lid_t + 2]);
    // tus pimi delikleri
    for (p=[btn1_pos, btn2_pos])
      translate([board_x + p[0], p[1], lid_bot - 1]) cylinder(d=pin_d + 0.4, h=lid_t + 2, $fn=32);
  }
}

// ---------- Gorsel yardimcilar (sadece montaj gorunumu) ----------
board_stl = "board_raw.stl";   // STEP'ten donusturulmus kart (sadece gorunum)
module dummy_parts() {
  if (board_stl != "") color("purple")
    translate([board_x, 0, pcb_bot]) rotate([0,0,-90]) translate([0, 12.475, 0]) rotate([180,0,0]) import(board_stl);
  if (board_stl == "") color("purple") translate([board_x - pcb_l/2, -pcb_w/2, pcb_bot]) cube([pcb_l, pcb_w, pcb_t]);
  if (board_stl == "") color("silver") translate([board_x + pcb_l/2 + usb_over - 7.3, -usb_w/2, pcb_top]) cube([7.3, usb_w, usb_h]);
  color("black") translate([0,0,ring_bot]) difference() {
    cylinder(r=ring_od/2, h=ring_t); translate([0,0,-1]) cylinder(r=ring_id/2, h=5);
  }
  for (i=[0:11]) rotate([0,0,i*30]) color("white")
    translate([(ring_od+ring_id)/4 - 2.5, -2.5, ring_top]) cube([5,5,led_h]);
}

// ---------- Base-Tilted yerlesimi ----------
stand_file = "Base-Tilted.stl";
st_c   = [57.96, 57.96];   // orta bosluk merkezi
st_floor = 2.0;            // bosluk tabani
module place() { translate([st_c[0], st_c[1], st_floor]) rotate([0,0,90]) children(); }  // USB -> kanal (+Y)

if (part == "base") base();
else if (part == "pins") for (i=[0,1]) translate([i*8, 0, lid_bot + lid_t + pin_up]) rotate([180,0,0]) pin();
else if (part == "assembly") {
  color("lightsteelblue") import(stand_file, convexity=6);
  place() { color("dimgray") base(); dummy_parts(); color("white", 0.55) lid();
            color("orange") for (p=[btn1_pos, btn2_pos]) translate([board_x + p[0], p[1], 0]) pin(); }
}
else if (part == "placed") place() base();
else if (part == "plug") place() translate([usb_face, -plug_w/2 + 0.3, usb_cz - plug_h/2 + 0.3]) cube([23, plug_w - 0.6, plug_h - 0.6]);
else if (part == "cable") place() translate([usb_face, 0, usb_cz]) rotate([0,90,0]) cylinder(d=3, h=60);
else if (part == "lid") translate([0,0,H_total]) rotate([180,0,0]) lid();   // baski yonu
else if (part == "box") {
  color("dimgray") base();
  dummy_parts();
  color("white", 0.55) lid();
}
