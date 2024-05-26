/*
Danger from the Deep - Open source submarine simulation
Copyright (C) 2003-2020  Thorsten Jordan, Luis Barrancos and others.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// torpedoes
// subsim (C)+(W) Thorsten Jordan. SEE LICENSE

#include "torpedo.hpp"

#include "datadirs.hpp"
#include "game.hpp"
#include "global_data.hpp"
#include "log.hpp"
#include "model.hpp"
#include "sensors.hpp"
#include "submarine.hpp"
#include "system_interface.hpp"

#include <utility>
using std::string;

torpedo::fuse::fuse(const xml_elem& parent, date /*equipdate*/)
{
    string modelstr = parent.attr("type");
    xml_doc doc(get_data_dir() + "objects/torpedoes/fuses.data");
    doc.load();
    xml_elem fs = doc.child("dftd-torpedo-fuses");
    if (!fs.has_child(modelstr)) {
        THROW(xml_error, "unknown fuse type!", parent.doc_name());
    }
    xml_elem f = fs.child(modelstr);
    string ts  = f.attr("type");
    if (ts == "impact") {
        type = IMPACT;
    } else if (ts == "inertial") {
        type = INERTIAL;
    } else if (ts == "influence") {
        type = INFLUENCE;
    } else {
        THROW(xml_error, "illegal fuse tyoe!", f.doc_name());
    }
    failure_probability = f.attrf("failure_probability");
}

/*
bool torpedo::fuse::handle_impact(angle impactangle) const
{
    // compute failure depending on angle, type and probability
    if (gm.randomf() < failure_probability)
        return false;
    return true;
}
*/

void torpedo::setup_data::load(const xml_elem& parent)
{
    primaryrange        = parent.attru("primaryrange");
    short_secondary_run = parent.attrb("short_secondary_run");
    initialturn_left    = parent.attrb("initialturn_left");
    turnangle           = angle(parent.attrf("turnangle"));
    lut_angle           = angle(parent.attrf("lut_angle"));
    torpspeed           = parent.attru("torpspeed");
    rundepth            = parent.attrf("rundepth");
    preheating          = parent.attrb("preheating");
}

void torpedo::setup_data::save(xml_elem& parent) const
{
    parent.set_attr(primaryrange, "primaryrange");
    parent.set_attr(short_secondary_run, "short_secondary_run");
    parent.set_attr(initialturn_left, "initialturn_left");
    parent.set_attr(turnangle.value(), "turnangle");
    parent.set_attr(lut_angle.value(), "lut_angle");
    parent.set_attr(torpspeed, "torpspeed");
    parent.set_attr(rundepth, "rundepth");
    parent.set_attr(preheating, "preheating");
}

torpedo::torpedo(
    date game_date,
    date equipment_date,
    object_store<model>& model_store,
    const xml_elem& parent,
    const setup_data& torpsetup)
    : ship(game_date, model_store, parent)
    , setup(torpsetup)
    , temperature(15)
    , // degrees C
    probability_of_rundepth_failure(0.2)
    , // basically high before mid 1942, fixme
    run_length(0)
    , steering_device_phase(0)
    , dive_planes(
          vector3(0, -3.5, 0 /*not used yet*/),
          1,
          20,
          0.25 * 0.1 /*area*/,
          40) // read consts from spec file, fixme
{
    // ------------ availability, check this first
    xml_elem eavailability = parent.child("availability");
    date availdt           = date(eavailability.attr("date"));
    if (equipment_date < availdt) {
        THROW(xml_error, "torpedo type not available at this date!", parent.doc_name());
    }

    set_skin_layout(model::default_layout);

    mass              = parent.child("weight").attrf();
    mass_inv          = 1 / mass;
    untertrieb        = parent.child("untertrieb").attrf();
    xml_elem ewarhead = parent.child("warhead");
    warhead_weight    = ewarhead.attrf("weight");
    string charge     = ewarhead.attr("charge");
    if (charge == "Ka") {
        warhead_type = Ka;
    } else if (charge == "Kb") {
        warhead_type = Kb;
    } else if (charge == "Kc") {
        warhead_type = Kc;
    } else if (charge == "Kd") {
        warhead_type = Kd;
    } else if (charge == "Ke") {
        warhead_type = Ke;
    } else if (charge == "Kf") {
        warhead_type = Kf;
    } else {
        // fixme: charges are atm numbers, should be replaced later...
        warhead_type = Ka;
        // THROW(xml_error, string("unknown charge type ")+charge,
        // parent.doc_name());
    }
    // ------------- arming
    xml_elem earming              = parent.child("arming");
    arming_distance               = -1;
    double latest_arming_distance = -1;            // just in case today's date is after
    date latest                   = date("1/1/1"); // the latest available period specified
    for (auto eperiod : earming.iterate("period")) {
        date from  = eperiod.attr("from");
        date until = eperiod.attr("until");
        if (until >= latest) {
            latest                 = until;
            latest_arming_distance = eperiod.attrf("runlength");
        }
        if (from <= equipment_date && equipment_date <= until) {
            arming_distance = eperiod.attrf("runlength");
            break;
        }
    }
    if (arming_distance < 0) {
        if (equipment_date >= latest) {
            arming_distance = latest_arming_distance;
        } else {
            THROW(
                xml_error,
                "no period subtags of arming that match current equipment "
                "date!",
                parent.doc_name());
        }
    }
    // ---------- fuse(s)
    xml_elem efuse = parent.child("fuse");
    fuse latest_fuse;
    latest = date("1/1/1");
    for (auto eperiod : efuse.iterate("period")) {
        date from  = eperiod.attr("from");
        date until = eperiod.attr("until");
        if (until >= latest) {
            latest      = until;
            latest_fuse = fuse(eperiod, equipment_date);
        }
        if (from <= equipment_date && equipment_date <= until) {
            fuse f(eperiod, equipment_date);
            if (f.type == fuse::IMPACT || f.type == fuse::INERTIAL) {
                contact_fuse = f;
            } else {
                magnetic_fuse = f;
            }
        }
    }
    if (contact_fuse.type == fuse::NONE && magnetic_fuse.type == fuse::NONE) {
        if (latest_fuse.type == fuse::IMPACT || latest_fuse.type == fuse::INERTIAL) {
            contact_fuse = latest_fuse;
        } else if (latest_fuse.type == fuse::INFLUENCE) {
            magnetic_fuse = latest_fuse;
        } else {
            THROW(xml_error, "no period subtags of fuse that match current equipment date!", parent.doc_name());
        }
    }
    // ----------- motion / steering device
    xml_elem emotion = parent.child("motion");
    unsigned hasfat  = emotion.attru("FAT");
    unsigned haslut  = emotion.attru("LUT");
    if (hasfat > 0) {
        if (haslut > 0) {
            THROW(xml_error, "steering device must be EITHER LuT OR FaT!", parent.doc_name());
        }
        steering_device = (hasfat == 1) ? FATI : FATII;
    } else if (haslut > 0) {
        // only difference between LUTI/II is maximum turn angle, LUT I 210
        // degrees, LUT II more (240?)
        steering_device = (haslut == 1) ? LUTI : LUTII;
    } else {
        steering_device = STRAIGHT;
    }
    // ------------ power and check of validity of torpspeed setting
    xml_elem epower  = parent.child("power");
    string powertype = epower.attr("type");
    if (powertype == "steam") {
        propulsion_type = STEAM;
        if ((hasfat != 0U) || (haslut != 0U)) {
            setup.torpspeed = NORMAL; // 30kts / slow G7a
        }
    } else if (powertype == "electric") {
        propulsion_type = ELECTRIC;
        if (setup.torpspeed != NORMAL && setup.torpspeed != PREHEATED) {
            setup.torpspeed = NORMAL;
        }
    } else if (powertype == "ingolin") {
        propulsion_type = INGOLIN;
        setup.torpspeed = NORMAL;
    } else {
        THROW(xml_error, "unknown power type!", parent.doc_name());
    }

    // ------------ sensors, fixme
    // ------------ ranges
    xml_elem eranges = parent.child("ranges");
    range[SLOW] = range[MEDIUM] = range[FAST] = 0.0;
    speed[SLOW] = speed[MEDIUM] = speed[FAST] = 0.0;
    for (auto erange : eranges.iterate("range")) {
        speedrange_types srt = NORMAL;
        // fixme: handle from/until tags and check against gm.get_date!
        if (erange.has_attr("preheated")) {
            if (erange.attrb("preheated")) {
                srt = PREHEATED;
            } else {
                srt = NORMAL;
            }
        } else if (erange.has_attr("throttle")) {
            if (erange.attr("throttle") == "slow") {
                srt = SLOW;
            } else if (erange.attr("throttle") == "medium") {
                srt = MEDIUM;
            } else if (erange.attr("throttle") == "fast") {
                srt = FAST;
            } else {
                THROW(xml_error, "illegal throttle attribute!", parent.doc_name());
            }
        } else {
            THROW(xml_error, "illegal speed/range type attributes!", parent.doc_name());
        }
        range[srt] = erange.attrf("distance");
        speed[srt] = kts2ms(erange.attrf("speed"));
    }

    // ------------ set ship turning values, fixme: read from files, more a
    // hack...
    rudder.max_angle      = 20;
    rudder.max_turn_speed = 40;
    // set turn rate here. With 0.6 a torpedo takes roughly 10 seconds to turn
    // 90 degrees. With that value the torpedo turn radius is ~98m. Maybe a bit
    // too much. fixme: only AI requests turn rate! either precompute it or
    // start with turn rate 0.1 and increase it by measured value e.g. measure
    // current turn speed in angles/sec and increase turn_rate. depends on speed
    // to...
    turn_rate = 0.6;
    // set rudder area
    rudder.area = 0.25 * 0.1 * 0.5; // diameter 0,53m, rudder ca. half height,
                                    // but parts of that, 10cm length

    size3d = vector3f(0.533, 7, 0.533); // diameter 53.3cm (21inch), length ~
                                        // 7m, fixme read from model file
    // mass = 1500; // 1.5tons, fixme read from spec file
    mass               = mymodel->get_base_mesh().volume * 1000.0;
    mass_inv           = 1.0 / mass;
    inertia_tensor     = mymodel->get_base_mesh().inertia_tensor * mass;
    inertia_tensor_inv = inertia_tensor.inverse();

    log_debug("torpedo mass now " << mass);
}

void torpedo::load(const xml_elem& parent)
{
    ship::load(parent);
    setup.load(parent.child("setup"));
    temperature                     = parent.child("temperature").attrf();
    probability_of_rundepth_failure = parent.child("probability_of_rundepth_failure").attrf();
    run_length                      = parent.child("run_length").attrf();
    steering_device_phase           = parent.child("steering_device_phase").attru();
    dive_planes.load(parent.child("dive_planes"));
}

void torpedo::save(xml_elem& parent) const
{
    ship::save(parent);
    xml_elem st = parent.add_child("setup");
    setup.save(st);
    parent.add_child("temperature").set_attr(temperature);
    parent.add_child("probability_of_rundepth_failure").set_attr(probability_of_rundepth_failure);
    parent.add_child("run_length").set_attr(run_length);
    parent.add_child("steering_device_phase").set_attr(steering_device_phase);
    xml_elem ed = parent.add_child("dive_planes");
    rudder.save(ed);
}

void torpedo::simulate(double delta_time, game& gm)
{
    if (!is_reference_ok()) {
        return;
    }

    /*
    log_debug("torpedo  " << this << " heading " << heading.value() << " should
    head to " << head_to.value() << " turn speed " << turn_velocity << "\n"
          << " position " << position << " orientation " << orientation << "
    run_length " << run_length << "\n"
          << " velo " << velocity << " turnvelo " << turn_velocity << "\n"
          << " delta t "<< delta_time << "linear_mom " << linear_momentum);
    */
    redetect_time = 1.0;
    ship::simulate(delta_time, gm);

    depth_steering_logic();
    dive_planes.simulate(delta_time);

    run_length += get_speed() * delta_time;
    if (run_length > get_range()) {
        // later: simulate slow sinking to the ground...
        // just set acceleration to zero, physic engine will do the rest
        // it doesn't sink to sea floor when doing this
        // alive_stat = inactive;
        kill();
        return;
    }

    // Torpedo starts to search for a target when the minimum save
    // distance for the warhead is passed.
    if (!sensors.empty() && run_length >= sensor_activation_distance) {
        const auto* target = gm.sonar_acoustical_torpedo_target(this);
        if (target != nullptr) {
            angle targetang(target->get_engine_noise_source() - get_pos().xy());
            bool turnright = get_heading().is_clockwise_nearer(targetang);
            head_to_course(targetang, static_cast<int>(!turnright));
        }
    }

    if (steering_device != STRAIGHT) {
        // Here we handle the special steering devices, FaT I/II and LuT I/II.
        // The devices have three phases:
        // 0 - initial straight run with angle to target
        // 1 - turning in one direction to new course and then running straight
        // 2 - turning in opposite direction to new course and then running
        // straight phase change happens because of run_length, from phase 0 to
        // phase 1, then 2, then 1 and 2 alternating. Angles between phases
        // differ between the devices and can be set up by the player.
        if (steering_device_phase == 0) {
            if (run_length >= setup.primaryrange) {
                log_debug(
                    "0: dev=" << steering_device << " short=" << setup.short_secondary_run
                              << " left=" << setup.initialturn_left);
                steering_device_phase = 1;
                if (steering_device == LUTI || steering_device == LUTII) {
                    // for LUT devices we turn now to the LUT main course
                    head_to_course(setup.lut_angle, 0 /* auto direction */, true /* small turn circle */);
                } else if (steering_device == FATII && setup.short_secondary_run) {
                    // for FAT II with short second turns, begin circling
                    set_rudder(setup.initialturn_left ? -1.0 : 1.0);
                } else {
                    // FAT I / FAT II long run: turn 180 degrees
                    head_to_course(
                        get_heading() + angle(180), setup.initialturn_left ? -1 : 1, false /* large turn circle */);
                }
            }
        } else if (steering_device_phase == 1) {
            auto phase = unsigned(floor((run_length - setup.primaryrange) / get_secondary_run_lenth()));
            if ((phase & 1) != 0U) {
                // phase change - FATII with short secondary turn changes
                // nothing, other setups turn and change phase
                if (steering_device != FATII || !setup.short_secondary_run) {
                    // first LUT turn is on phase 1->2, so invert turn direction
                    bool is_lut    = steering_device == LUTI || steering_device == LUTII;
                    bool turn_left = is_lut ? setup.initialturn_left : !setup.initialturn_left;
                    // LUT device sets course according to main course, heading
                    // should have reached that course here, so we can use
                    // get_heading() instead of setup.lut_angle - fixme test
                    // this!
                    log_debug(
                        "1: dev=" << steering_device << " short=" << setup.short_secondary_run
                                  << " left=" << setup.initialturn_left << " turn=" << turn_left);
                    head_to_course(
                        get_heading() + setup.turnangle, turn_left ? -1 : 1, is_lut /* hard rudder for lut*/);
                    steering_device_phase = 2;
                }
            }
        } else {
            // steering_device_phase = 2 here
            auto phase = unsigned(floor((run_length - setup.primaryrange) / get_secondary_run_lenth()));
            if ((phase & 1) == 0) {
                // first LUT turn is on phase 1->2, so invert turn direction,
                // invert general because of phase
                bool is_lut    = steering_device == LUTI || steering_device == LUTII;
                bool turn_left = is_lut ? !setup.initialturn_left : setup.initialturn_left;
                log_debug(
                    "2: dev=" << steering_device << " short=" << setup.short_secondary_run
                              << " left=" << setup.initialturn_left << " turn=" << turn_left);
                head_to_course(get_heading() + setup.turnangle, turn_left ? -1 : 1, is_lut /* hard rudder for lut*/);
                steering_device_phase = 1;
            }
        }
    }

    // check for collisions with other subs or ships
    if (run_length > 10) { // avoid collision with parent after initial creation
        bool runlengthfailure = (run_length < arming_distance);
        if (gm.check_torpedo_hit(this, runlengthfailure)) {
            kill();
        }
    }
}

void torpedo::compute_force_and_torque(vector3& F, vector3& T, game& gm) const
{
    ship::compute_force_and_torque(F, T, gm);

    // drag by stern dive rudder

    vector3 Fdr;
    vector3 Tdr;
    double flowforce = get_throttle_accel() * mass * rudder.deflect_factor();
    double finalflowforce =
        dive_planes.compute_force_and_torque(Fdr, Tdr, get_local_velocity(), constant::SEA_WATER_DENSITY, flowforce);
    // we limit torque here to avoid too much turning of the torpedo.
    // Otherwise small dive plane movements would cause large turning, not only
    // depth changes (by the laws of physics). This trick simulates
    // the stabilizing work of fins
    Tdr.x = 0.01;

    // when stern rudder is not at angle 0, some force points orthogonal to the
    // rudder (stern_depth_rudder.deflect_factor), so less force is available
    // for forward movement of torpedo. So subtract from forward force what does
    // not bypass the rudder.
    // log_debug("Fdr=" << Fdr << " Tdr=" << Tdr);
    Fdr.y += finalflowforce - flowforce;

    F += orientation.rotate(Fdr);
    T += orientation.rotate(Tdr);
}

void torpedo::depth_steering_logic()
{
    double depthdiff = position.z - (-setup.rundepth);
    double error0    = depthdiff;
    double error1    = dive_planes.max_angle / dive_planes.max_turn_speed * local_velocity.z * 1.0;
    double error2    = 0; //-rudder_pos/max_rudder_turn_speed * turn_velocity;
    double error     = error0 + error1 + error2;
    // DBGOUT8(position.z,depthdiff, local_velocity.z, dive_planes.angle,
    // error0, error1, error2, error);
    double rd            = myclamp(error, -5.0, 5.0);
    dive_planes.to_angle = dive_planes.max_angle * rd / 5.0;
}

auto torpedo::get_throttle_speed() const -> double
{
    return run_length > get_range() ? 0.0 : get_max_speed();
}

auto torpedo::get_secondary_run_lenth() const -> double
{
    if (setup.short_secondary_run) {
        switch (steering_device) {
            case FATI:
                return 1200.0;
            case FATII:
                return 100000.0; // infinite, because turning in circles
            case LUTI:
            case LUTII:
                return 1350.0;
            default:
                return 0.0;
        }
    } else {
        switch (steering_device) {
            case FATI:
            case FATII:
                return 1900.0;
            case LUTI:
            case LUTII:
                return 3840.0;
            default:
                return 0.0;
        }
    }
}

auto torpedo::get_turn_drag_area() const -> double
{
    // torpedo is fully under water, so use full cross section
    return mymodel->get_cross_section(90.0);
}

void torpedo::launch(const vector3& launchpos, angle parenthdg)
{
    position          = launchpos;
    orientation       = quaternion::rot(-parenthdg.value(), 0, 0, 1);
    max_speed_forward = get_torp_speed();
    linear_momentum   = orientation.rotate(vector3(0, max_speed_forward * mass, 0)); // fixme: get from parent
    angular_momentum  = vector3();                                                   // fixme: get from parent
    compute_helper_values();
    run_length    = 0;
    turn_velocity = 0;
}

#if 0
//fixme beim laden mitbenutzen
void torpedo::create_sensor_array ( types t )
{
    switch ( t )
    {
        case T4:	// fixme
            set_sensor ( passive_sonar_system, new passive_sonar_sensor (
                passive_sonar_sensor::passive_sonar_type_tt_t5 ) );
            break;
        case T5:
            set_sensor ( passive_sonar_system, new passive_sonar_sensor (
                passive_sonar_sensor::passive_sonar_type_tt_t5 ) );
            break;
        case T11:
            set_sensor ( passive_sonar_system, new passive_sonar_sensor (
                passive_sonar_sensor::passive_sonar_type_tt_t11 ) );
            break;
    }
}
#endif

auto torpedo::get_hit_points() const -> unsigned // awful, useless, replace, fixme
{
    return 100; // G7A_HITPOINTS;//fixme
}

auto torpedo::get_range() const -> double
{
    switch (propulsion_type) {
        case STEAM:
            return range[setup.torpspeed];
        case ELECTRIC: {
            // varies between 15 and 30
            double s = myclamp((temperature - 15) / 15, 0.0, 1.0);
            return helper::interpolate(range[NORMAL], range[PREHEATED], s);
        }
        case INGOLIN: // Walther turbine
        default:
            return range[NORMAL];
    }
}

auto torpedo::get_torp_speed() const -> double
{
    switch (propulsion_type) {
        case STEAM:
            return speed[setup.torpspeed];
        case ELECTRIC: {
            // varies between 15 and 30
            double s = myclamp((temperature - 15) / 15, 0.0, 1.0);
            return helper::interpolate(speed[NORMAL], speed[PREHEATED], s);
        }
        case INGOLIN: // Walther turbine
        default:
            return speed[NORMAL];
    }
}

auto torpedo::test_contact_fuse(game& gm) const -> bool
{
    // compute if contact fuse fails (if fuse type is NONE, probability is 1.0,
    // so it fails always)
    return gm.randomf() > contact_fuse.failure_probability;
}

auto torpedo::test_magnetic_fuse(game& gm) const -> bool
{
    // compute if magnetic fuse fails (if fuse type is NONE, probability is 1.0,
    // so it fails always)
    return gm.randomf() > magnetic_fuse.failure_probability;
}
