use super::*;
use orb_engine::dist::{self, Distortion};

fn ctx<'a>(tables: &'a Tables) -> BlockContext<'a> {
    BlockContext {
        tables,
        meter_index: 0,
        tempo: DUMMY_TEMPO_MILLI,
        tick: 0,
        stack_word: None,
    }
}

fn apply(d: &mut Distortion, params: Params) {
    d.set_param(dist::AMOUNT as u8, params.amount.min(127));
    d.set_param(dist::SHAPE as u8, params.shape.min(127));
    d.set_param(dist::ENABLED as u8, 1);
}

/// A few blocks through the wrapper equal `Distortion::process` on each channel, delayed by 64
/// samples. The first block is silent.
#[test]
fn wrapper_matches_distortion_process() {
    let params = Params::default();
    let nblocks = 4;
    let n = nblocks * BLOCK;
    let mut input_l = vec![0.0f32; n];
    let mut input_r = vec![0.0f32; n];
    for i in 0..n {
        input_l[i] = ((i % 17) as f32) * 0.02 - 0.1;
        input_r[i] = ((i % 13) as f32) * -0.015 + 0.05;
    }

    let mut wrap_l = input_l.clone();
    let mut wrap_r = input_r.clone();
    let mut wrap = DistStereo::new();
    wrap.process(&mut wrap_l, &mut wrap_r, params);

    assert!(wrap_l[..BLOCK].iter().all(|&x| x == 0.0));
    assert!(wrap_r[..BLOCK].iter().all(|&x| x == 0.0));

    let tables = Tables::new();
    let mut left = Distortion::new();
    let mut right = Distortion::new();
    apply(&mut left, params);
    apply(&mut right, params);

    for k in 0..nblocks - 1 {
        let mut bl: Block = input_l[k * BLOCK..(k + 1) * BLOCK].try_into().unwrap();
        let mut br: Block = input_r[k * BLOCK..(k + 1) * BLOCK].try_into().unwrap();
        left.process(&mut bl, &ctx(&tables));
        right.process(&mut br, &ctx(&tables));
        assert_eq!(
            wrap_l[(k + 1) * BLOCK..(k + 2) * BLOCK],
            bl,
            "left block {k}"
        );
        assert_eq!(
            wrap_r[(k + 1) * BLOCK..(k + 2) * BLOCK],
            br,
            "right block {k}"
        );
    }
}

/// Shape 0 is the 1.5 hard clip; a non-zero shape uses the waveshaper. Same input, different out.
#[test]
fn shape_0_clip_differs_from_the_shaper() {
    let n = 2 * BLOCK;
    let mut input = vec![0.0f32; n];
    for i in 0..n {
        input[i] = ((i % 11) as f32) * 0.08 - 0.4;
    }

    let mut clip_l = input.clone();
    let mut clip_r = input.clone();
    DistStereo::new().process(
        &mut clip_l,
        &mut clip_r,
        Params {
            amount: 70,
            shape: 0,
            bypass: 0,
        },
    );

    let mut shape_l = input.clone();
    let mut shape_r = input.clone();
    DistStereo::new().process(
        &mut shape_l,
        &mut shape_r,
        Params {
            amount: 70,
            shape: 64,
            bypass: 0,
        },
    );

    assert_ne!(&clip_l[BLOCK..], &shape_l[BLOCK..]);
    assert_ne!(&clip_r[BLOCK..], &shape_r[BLOCK..]);
}

/// Bypass is delayed dry. The engine counter does not advance while skipped.
#[test]
fn bypass_is_delayed_dry() {
    let params = Params {
        amount: 70,
        shape: 64,
        bypass: 1,
    };
    let n = 2 * BLOCK;
    let mut input_l = vec![0.0f32; n];
    let mut input_r = vec![0.0f32; n];
    for i in 0..n {
        input_l[i] = (i as f32) * 0.001;
        input_r[i] = -(i as f32) * 0.002;
    }

    let mut wrap_l = input_l.clone();
    let mut wrap_r = input_r.clone();
    let mut wrap = DistStereo::new();
    wrap.process(&mut wrap_l, &mut wrap_r, params);

    assert!(wrap_l[..BLOCK].iter().all(|&x| x == 0.0));
    assert!(wrap_r[..BLOCK].iter().all(|&x| x == 0.0));
    assert_eq!(&wrap_l[BLOCK..], &input_l[..BLOCK]);
    assert_eq!(&wrap_r[BLOCK..], &input_r[..BLOCK]);
    assert_eq!(wrap.left().counter, 0);
    assert_eq!(wrap.left().live[dist::AMOUNT], dist::DEFAULTS[dist::AMOUNT]);
}

/// A short buffer leaves a remainder; the next call finishes the block.
#[test]
fn short_buffer_leaves_a_remainder() {
    let params = Params::default();
    let mut p = DistStereo::new();
    let mut l = [0.2f32; 32];
    let mut r = [0.2f32; 32];
    p.process(&mut l, &mut r, params);
    assert_eq!(p.blocks_processed(), 0);
    assert_eq!(p.fill(), 32);

    let mut l = [0.2f32; 32];
    let mut r = [0.2f32; 32];
    p.process(&mut l, &mut r, params);
    assert_eq!(p.blocks_processed(), 1);
    assert_eq!(p.fill(), 0);
}

/// Amount and shape 128 and 255 are treated as 127.
#[test]
fn amount_and_shape_above_127_are_clamped() {
    for v in [128u8, 255] {
        let mut p = DistStereo::new();
        let mut l = [0.5f32; BLOCK];
        let mut r = [0.5f32; BLOCK];
        p.process(
            &mut l,
            &mut r,
            Params {
                amount: v,
                shape: v,
                bypass: 0,
            },
        );
        assert_eq!(p.left().live[dist::AMOUNT], 127);
        assert_eq!(p.left().live[dist::SHAPE], 127);
    }
}
