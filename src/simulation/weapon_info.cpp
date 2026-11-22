#include "weapon_info.h"

namespace apache::simulation {
std::array<weapon_info, 55> weapon_definitions() {
  // winfo.c, initialise_weapon_data_ (0x451ee0). Definitions include its
  // like_() inheritance and final assignments, in original world units.
  std::array<weapon_info, 55> result;
  {                                                                            // 0: None
    auto &w{result[0]};
    w.burst_size = 1;
    w.decoy_denominator = 1;
    w.minimum_pitch = -1073741824;
    w.maximum_pitch = 1073741824;
    w.heading_limit = INT32_MIN;
    w.minimum_range = 1;
    w.maximum_range = 1;
    w.maximum_speed = 1;
    w.launch_speed = 1;
    w.tracking_or_dispersion = 11930465;
    w.name = "None";
  }
  {                                                                            // 1: Chain Gun
    result[1] = result[0];
    auto &w{result[1]};
    w.movement_class = 1;
    w.flags = 3;
    w.alternate_penetration = 2;
    w.burst_size = 20;
    w.weapon_role = 8;
    w.damage = 4;
    w.burst_pause_ticks = 600;
    w.minimum_pitch = -715827883;
    w.maximum_pitch = 238609294;
    w.heading_limit = 715827883;
    w.penetration = 1;
    w.minimum_range = 0;
    w.maximum_range = 1024000;
    w.maximum_speed = 768000;
    w.launch_speed = 768000;
    w.model = 7;
    w.fire_interval_ticks = 10;
    w.name = "Chain Gun";
  }
  {                                                                            // 2: AGM-114A
    result[2] = result[0];
    auto &w{result[2]};
    w.movement_class = 2;
    w.guidance = 1;
    w.flags = 4;
    w.alternate_penetration = 4;
    w.weapon_role = 4;
    w.damage = 50;
    w.motor_acceleration = 71680;
    w.motor_ticks = 1000;
    w.guidance_start_distance = 52480;
    w.blast_radius = 5120;
    w.minimum_pitch = 0;
    w.maximum_pitch = 0;
    w.heading_limit = 0;
    w.penetration = 2;
    w.minimum_range = 76800;
    w.maximum_range = 4608000;
    w.minimum_speed = 128000;
    w.maximum_speed = 332800;
    w.launch_speed = 7680;
    w.model = 24;
    w.target_box_half_size = 567920;
    w.fire_interval_ticks = 100;
    w.lateral_acceleration = 53248;
    w.tracking_or_dispersion = 238609294;
    w.drag_coefficient = 0x1.c6ed6dd298a81p-25;
    w.name = "AGM-114A";
  }
  {                                                                            // 3: AGM-114B
    result[3] = result[2];
    auto &w{result[3]};
    w.guidance = 2;
    w.decoy_denominator = 20;
    w.maximum_range = 6912000;
    w.name = "AGM-114B";
  }
  {                                                                            // 4: HydraM255
    result[4] = result[1];
    auto &w{result[4]};
    w.flags = 1;
    w.alternate_penetration = 1;
    w.burst_size = 2;
    w.weapon_role = 4;
    w.damage = 25;
    w.motor_acceleration = 281600;
    w.motor_ticks = 600;
    w.burst_pause_ticks = 200;
    w.blast_radius = 12800;
    w.minimum_pitch = -119304647;
    w.maximum_pitch = 0;
    w.heading_limit = 59652324;
    w.minimum_range = 384000;
    w.maximum_range = 4608000;
    w.minimum_speed = 256000;
    w.maximum_speed = 839680;
    w.launch_speed = 7680;
    w.model = 138;
    w.fire_interval_ticks = 100;
    w.drag_coefficient = 0x1.7270e0bdc572ep-23;
    w.name = "HydraM255";
  }
  {                                                                            // 5: HydraM261
    result[5] = result[4];
    auto &w{result[5]};
    w.alternate_penetration = 2;
    w.damage = 50;
    w.blast_radius = 7680;
    w.penetration = 2;
    w.name = "HydraM261";
  }
  {                                                                            // 6: Stinger
    result[6] = result[3];
    auto &w{result[6]};
    w.guidance = 3;
    w.alternate_penetration = 2;
    w.weapon_role = 3;
    w.motor_acceleration = 268800;
    w.motor_ticks = 600;
    w.guidance_start_distance = 108480;
    w.blast_radius = 12800;
    w.minimum_range = 168960;
    w.maximum_range = 4224000;
    w.minimum_speed = 204800;
    w.maximum_speed = 768000;
    w.model = 133;
    w.lateral_acceleration = 81920;
    w.drag_coefficient = 0x1.0b8a696c9cd32p-23;
    w.name = "Stinger";
  }
  {                                                                            // 7: Chaff
    result[7] = result[0];
    auto &w{result[7]};
    w.movement_class = 4;
    w.flags = 2;
    w.weapon_role = 9;
    w.minimum_range = 0;
    w.maximum_range = 20480;
    w.model = 478;
    w.fire_interval_ticks = 400;
    w.tracking_or_dispersion = 1280;
    w.drag_coefficient = 0x1.81e03f705857bp-20;
    w.name = "Chaff";
  }
  {                                                                            // 8: Flare
    result[8] = result[7];
    auto &w{result[8]};
    w.damage = 1;
    w.motor_ticks = 24000;
    w.maximum_range = 25600;
    w.model = 477;
    w.tracking_or_dispersion = 2560;
    w.drag_coefficient = 0x1.edebd6525c993p-26;
    w.name = "Flare";
  }
  {                                                                            // 9: !!Erf!!
    result[9] = result[6];
    auto &w{result[9]};
    w.maximum_range = 12800000;
    w.model = 670;
    w.fire_interval_ticks = 50;
    w.name = "!!Erf!!";
  }
  {                                                                            // 10: Cannon Pod
    result[10] = result[1];
    auto &w{result[10]};
    w.alternate_penetration = 3;
    w.damage = 6;
    w.maximum_range = 1152000;
    w.name = "Cannon Pod";
  }
  {                                                                            // 11: KMGU Pod
    result[11] = result[4];
    auto &w{result[11]};
    w.movement_class = 4;
    w.flags = 3;
    w.alternate_penetration = 0;
    w.weapon_role = 6;
    w.damage = 0;
    w.motor_acceleration = 0;
    w.motor_ticks = 20000;
    w.blast_radius = 0;
    w.penetration = 0;
    w.minimum_range = 0;
    w.maximum_range = 13516800;
    w.maximum_speed = 128000;
    w.model = 741;
    w.fire_interval_ticks = 200;
    w.tracking_or_dispersion = 8192;
    w.name = "KMGU Pod";
  }
  {                                                                            // 12: FAB500 Bomb
    result[12] = result[11];
    auto &w{result[12]};
    w.alternate_penetration = 2;
    w.damage = 255;
    w.blast_radius = 44800;
    w.penetration = 1;
    w.model = 668;
    w.name = "FAB500 Bomb";
  }
  {                                                                            // 13: FAB250 Bomb
    result[13] = result[11];
    auto &w{result[13]};
    w.alternate_penetration = 1;
    w.damage = 100;
    w.blast_radius = 32000;
    w.model = 669;
    w.name = "FAB250 Bomb";
  }
  {                                                                            // 14: OFAB250 Bomb
    result[14] = result[12];
    auto &w{result[14]};
    w.damage = 100;
    w.penetration = 0;
    w.model = 674;
    w.name = "OFAB250 Bomb";
  }
  {                                                                            // 15: ZAB500 Bomb
    result[15] = result[11];
    auto &w{result[15]};
    w.damage = 255;
    w.blast_radius = 76800;
    w.model = 673;
    w.name = "ZAB500 Bomb";
  }
  {                                                                            // 16: 80mm Rocket
    result[16] = result[4];
    auto &w{result[16]};
    w.alternate_penetration = 3;
    w.damage = 70;
    w.penetration = 2;
    w.minimum_range = 256000;
    w.launch_speed = 42240;
    w.model = 676;
    w.fire_interval_ticks = 50;
    w.name = "80mm Rocket";
  }
  {                                                                            // 17: 57mm Rocket
    result[17] = result[5];
    auto &w{result[17]};
    w.damage = 35;
    w.penetration = 1;
    w.minimum_range = 128000;
    w.launch_speed = 42240;
    w.model = 675;
    w.fire_interval_ticks = 50;
    w.name = "57mm Rocket";
  }
  {                                                                            // 18: Spiral Tube
    result[18] = result[2];
    auto &w{result[18]};
    w.maximum_range = 5888000;
    w.launch_speed = 42240;
    w.model = 723;
    w.fire_interval_ticks = 50;
    w.name = "Spiral Tube";
  }
  {                                                                            // 19: !!Erf!!
    result[19] = result[1];
    auto &w{result[19]};
    w.alternate_penetration = 1;
    w.damage = 2;
    w.penetration = 0;
    w.name = "!!Erf!!";
  }
  {                                                                            // 20: Tank Gun
    result[20] = result[0];
    auto &w{result[20]};
    w.movement_class = 3;
    w.alternate_penetration = 4;
    w.weapon_role = 7;
    w.damage = 40;
    w.blast_radius = 25600;
    w.penetration = 2;
    w.minimum_range = 25600;
    w.maximum_range = -41856;
    w.maximum_speed = 768000;
    w.launch_speed = 896000;
    w.fire_interval_ticks = 2000;
    w.tracking_or_dispersion = 38400;
    w.name = "Tank Gun";
  }
  {                                                                            // 21: MLRS
    result[21] = result[20];
    auto &w{result[21]};
    w.flags = 3;
    w.alternate_penetration = 2;
    w.burst_size = 12;
    w.damage = 100;
    w.burst_pause_ticks = 240000;
    w.maximum_range = 27033600;
    w.maximum_speed = 256000;
    w.launch_speed = 1;
    w.fire_interval_ticks = 200;
    w.tracking_or_dispersion = 153600;
    w.name = "MLRS";
  }
  {                                                                            // 22: !!Erf!!
    result[22] = result[16];
    auto &w{result[22]};
    w.fire_interval_ticks = 1000;
    w.name = "!!Erf!!";
  }
  {                                                                            // 23: AAA
    result[23] = result[1];
    auto &w{result[23]};
    w.burst_size = 10;
    w.weapon_role = 2;
    w.burst_pause_ticks = 1600;
    w.minimum_pitch = -1073741824;
    w.maximum_pitch = 1073741824;
    w.heading_limit = INT32_MIN;
    w.penetration = 0;
    w.minimum_range = 128000;
    w.maximum_range = 1344000;
    w.model = 345;
    w.name = "AAA";
  }
  {                                                                            // 24: SAM-Radar
    result[24] = result[3];
    auto &w{result[24]};
    w.flags = 0;
    w.alternate_penetration = 2;
    w.weapon_role = 1;
    w.decoy_denominator = 10;
    w.motor_acceleration = 256000;
    w.guidance_start_distance = 236800;
    w.blast_radius = 15360;
    w.minimum_pitch = 363879173;
    w.maximum_pitch = 1073741824;
    w.heading_limit = INT32_MIN;
    w.penetration = 0;
    w.minimum_range = 1280000;
    w.maximum_range = 6400000;
    w.maximum_speed = 768000;
    w.launch_speed = 76800;
    w.model = 530;
    w.target_box_half_size = 0;
    w.fire_interval_ticks = 12000;
    w.lateral_acceleration = 81920;
    w.tracking_or_dispersion = 357913941;
    w.name = "SAM-Radar";
  }
  {                                                                            // 25: SSM
    result[25] = result[21];
    auto &w{result[25]};
    w.alternate_penetration = 4;
    w.weapon_role = 5;
    w.damage = 255;
    w.minimum_range = 67584000;
    w.maximum_range = 235192320;
    w.fire_interval_ticks = 360000;
    w.tracking_or_dispersion = 76800;
    w.name = "SSM";
  }
  {                                                                            // 26: !!Erf!!
    result[26] = result[25];
    auto &w{result[26]};
    w.flags = 0;
    w.alternate_penetration = 2;
    w.fire_interval_ticks = 6000;
    w.tracking_or_dispersion = 768000;
    w.name = "!!Erf!!";
  }
  {                                                                            // 27: Artillery
    result[27] = result[21];
    auto &w{result[27]};
    w.flags = 2;
    w.blast_radius = 20480;
    w.maximum_range = 19200000;
    w.maximum_speed = 691200;
    w.launch_speed = 691200;
    w.fire_interval_ticks = 6000;
    w.tracking_or_dispersion = 76800;
    w.name = "Artillery";
  }
  {                                                                            // 28: Bomb
    result[28] = result[21];
    auto &w{result[28]};
    w.alternate_penetration = 3;
    w.burst_size = 10;
    w.weapon_role = 6;
    w.burst_pause_ticks = 180000;
    w.blast_radius = 51200;
    w.penetration = 4;
    w.minimum_range = 0;
    w.maximum_range = 13516800;
    w.maximum_speed = 128000;
    w.fire_interval_ticks = 10;
    w.tracking_or_dispersion = 38400;
    w.name = "Bomb";
  }
  {                                                                            // 29: L.G.Bomb
    result[29] = result[28];
    auto &w{result[29]};
    w.movement_class = 2;
    w.guidance = 1;
    w.flags = 2;
    w.alternate_penetration = 2;
    w.burst_size = 1;
    w.motor_acceleration = 8192;
    w.motor_ticks = 20000;
    w.burst_pause_ticks = 0;
    w.minimum_pitch = -1193046471;
    w.maximum_pitch = 1193046471;
    w.penetration = 2;
    w.maximum_range = 135168000;
    w.launch_speed = 0;
    w.model = 24;
    w.fire_interval_ticks = 50;
    w.lateral_acceleration = 204800;
    w.tracking_or_dispersion = 477218588;
    w.name = "L.G.Bomb";
  }
  {                                                                            // 30: Cannon
    result[30] = result[1];
    auto &w{result[30]};
    w.flags = 7;
    w.burst_size = 5;
    w.damage = 2;
    w.burst_pause_ticks = 200;
    w.blast_radius = 256;
    w.minimum_pitch = 35791394;
    w.maximum_pitch = 35791394;
    w.heading_limit = 35791394;
    w.penetration = 2;
    w.maximum_range = 640000;
    w.tracking_or_dispersion = 59652324;
    w.name = "Cannon";
  }
  {                                                                            // 31: AntiTank
    result[31] = result[3];
    auto &w{result[31]};
    w.minimum_pitch = -357913941;
    w.maximum_pitch = 357913941;
    w.heading_limit = 357913941;
    w.maximum_range = 2560000;
    w.fire_interval_ticks = 6000;
    w.name = "AntiTank";
  }
  {                                                                            // 32: ZSU23-AAA
    result[32] = result[23];
    auto &w{result[32]};
    w.maximum_range = 2100000;
    w.name = "ZSU23-AAA";
  }
  {                                                                            // 33: M163-AAA
    result[33] = result[23];
    auto &w{result[33]};
    w.maximum_range = 4992000;
    w.name = "M163-AAA";
  }
  {                                                                            // 34: SA9-SAM
    result[34] = result[24];
    auto &w{result[34]};
    w.guidance = 3;
    w.minimum_range = 672000;
    w.maximum_range = 4992000;
    w.maximum_speed = 409600;
    w.name = "SA9-SAM";
  }
  {                                                                            // 35: SA6-SAM
    result[35] = result[24];
    auto &w{result[35]};
    w.motor_ticks = 4400;
    w.minimum_range = 672000;
    w.maximum_range = 6144000;
    w.maximum_speed = 793600;
    w.lateral_acceleration = 61440;
    w.name = "SA6-SAM";
  }
  {                                                                            // 36: M48-SAM
    result[36] = result[24];
    auto &w{result[36]};
    w.minimum_range = 420000;
    w.maximum_range = 6912000;
    w.name = "M48-SAM";
  }
  {                                                                            // 37: Patriot
    result[37] = result[24];
    auto &w{result[37]};
    w.name = "Patriot";
  }
  {                                                                            // 38: Rapier
    result[38] = result[24];
    auto &w{result[38]};
    w.name = "Rapier";
  }
  {                                                                            // 39: SA8-SAM
    result[39] = result[24];
    auto &w{result[39]};
    w.minimum_range = 1344000;
    w.maximum_range = 10080000;
    w.maximum_speed = 368640;
    w.name = "SA8-SAM";
  }
  {                                                                            // 40: SA13-SAM
    result[40] = result[36];
    auto &w{result[40]};
    w.maximum_range = 8400000;
    w.maximum_speed = 368640;
    w.name = "SA13-SAM";
  }
  {                                                                            // 41: D30 Shell
    result[41] = result[27];
    auto &w{result[41]};
    w.maximum_range = 12936000;
    w.fire_interval_ticks = 1600;
    w.name = "D30 Shell";
  }
  {                                                                            // 42: Sidewinder AAM
    result[42] = result[6];
    auto &w{result[42]};
    w.guidance = 2;
    w.maximum_range = 14868480;
    w.maximum_speed = 512000;
    w.name = "Sidewinder AAM";
  }
  {                                                                            // 43: Sparrow AAM
    result[43] = result[42];
    auto &w{result[43]};
    w.maximum_range = 36480000;
    w.maximum_speed = 819200;
    w.name = "Sparrow AAM";
  }
  {                                                                            // 44: R27 AAM
    result[44] = result[6];
    auto &w{result[44]};
    w.guidance = 2;
    w.maximum_range = 32440320;
    w.name = "R27 AAM";
  }
  {                                                                            // 45: R73 AAM
    result[45] = result[6];
    auto &w{result[45]};
    w.name = "R73 AAM";
  }
  {                                                                            // 46: R60 AAM
    result[46] = result[42];
    auto &w{result[46]};
    w.maximum_range = 8448000;
    w.maximum_speed = 614400;
    w.fire_interval_ticks = 1000;
    w.name = "R60 AAM";
  }
  {                                                                            // 47: 155mm Howitzer
    result[47] = result[27];
    auto &w{result[47]};
    w.maximum_range = 15120000;
    w.fire_interval_ticks = 2000;
    w.name = "155mm Howitzer";
  }
  {                                                                            // 48: MiniGun
    result[48] = result[1];
    auto &w{result[48]};
    w.minimum_pitch = -119304647;
    w.maximum_pitch = 119304647;
    w.heading_limit = 119304647;
    w.penetration = 0;
    w.maximum_range = 384000;
    w.name = "MiniGun";
  }
  {                                                                            // 49: Styx SSM
    result[49] = result[31];
    auto &w{result[49]};
    w.damage = 150;
    w.blast_radius = 25600;
    w.minimum_pitch = -238609294;
    w.heading_limit = 2028179001;
    w.minimum_range = 675840;
    w.maximum_range = 24887296;
    w.maximum_speed = 256000;
    w.fire_interval_ticks = 2000;
    w.tracking_or_dispersion = 2028179001;
    w.name = "Styx SSM";
  }
  {                                                                            // 50: Standard SAM
    result[50] = result[24];
    auto &w{result[50]};
    w.maximum_range = 38886400;
    w.name = "Standard SAM";
  }
  {                                                                            // 51: Harpoon SSM
    result[51] = result[49];
    auto &w{result[51]};
    w.alternate_penetration = 2;
    w.blast_radius = 19200;
    w.maximum_range = 93327360;
    w.name = "Harpoon SSM";
  }
  {                                                                            // 52: SA7-SAM
    result[52] = result[34];
    auto &w{result[52]};
    w.damage = 30;
    w.motor_ticks = 400;
    w.minimum_pitch = 357913941;
    w.penetration = 1;
    w.minimum_range = 102400;
    w.maximum_range = 1433600;
    w.maximum_speed = 307200;
    w.model = 790;
    w.name = "SA7-SAM";
  }
  {                                                                            // 53: !!Erf!!
    result[53] = result[47];
    auto &w{result[53]};
    w.maximum_range = 18900000;
    w.name = "!!Erf!!";
  }
  {                                                                            // 54: unnamed
    result[54] = result[21];
    auto &w{result[54]};
    w.weapon_role = 5;
    w.damage = 150;
    w.blast_radius = 19200;
    w.fire_interval_ticks = 120000;
    w.tracking_or_dispersion = 51200;
    w.name = "";
  }
  return result;
}
} // namespace apache::simulation
