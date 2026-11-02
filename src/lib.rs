//! Stereo wrapper around [`orb_engine::dist::Distortion`].
//!
//! Two distortion instances share one [`Tables`]. Audio is processed in 64-sample blocks at
//! 44100 Hz, matching the original insert. Output is delayed by one block so a host sees a
//! constant 64-sample latency. The C API takes buffers already at 44100 Hz; resampling is the
//! JUCE plugin's job.
//!
//! Distortion has no tempo, pattern, or song clock. `BlockContext` is filled with dummy
//! `tempo: 120_000` and `tick: 0`.

use orb_engine::dist::{self, Distortion};
use orb_engine::frame::{BLOCK, Block, BlockContext, InsertEffect, SongDevice};
use orb_engine::tables::Tables;

/// Engine (and original) sample rate. The waveshaper and clip paths have no sample-rate
/// coefficients; this is the rate they were designed for.
pub const ENGINE_RATE: f64 = 44100.0;
/// Dummy tempo passed into [`BlockContext`]. Distortion does not read it.
const DUMMY_TEMPO_MILLI: u32 = 120_000;

/// Panel parameters applied at the start of each 64-sample block.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub struct Params {
    pub amount: u8,
    pub shape: u8,
    pub bypass: u8,
}

impl Default for Params {
    fn default() -> Self {
        let d = dist::DEFAULTS;
        Self {
            amount: d[dist::AMOUNT],
            shape: d[dist::SHAPE],
            bypass: 0,
        }
    }
}

/// Stereo distortion: two independent instances, a 64-sample delay.
pub struct DistStereo {
    tables: Tables,
    left: Distortion,
    right: Distortion,
    in_l: Block,
    in_r: Block,
    delay_l: Block,
    delay_r: Block,
    fill: usize,
    blocks_processed: u64,
}

impl Default for DistStereo {
    fn default() -> Self {
        Self::new()
    }
}

impl DistStereo {
    pub fn new() -> Self {
        Self {
            tables: Tables::new(),
            left: Distortion::new(),
            right: Distortion::new(),
            in_l: [0.0; BLOCK],
            in_r: [0.0; BLOCK],
            delay_l: [0.0; BLOCK],
            delay_r: [0.0; BLOCK],
            fill: 0,
            blocks_processed: 0,
        }
    }

    /// Clears distortion memory and the delay. Tables stay.
    pub fn reset(&mut self) {
        self.left = Distortion::new();
        self.right = Distortion::new();
        self.in_l = [0.0; BLOCK];
        self.in_r = [0.0; BLOCK];
        self.delay_l = [0.0; BLOCK];
        self.delay_r = [0.0; BLOCK];
        self.fill = 0;
        self.blocks_processed = 0;
    }

    pub fn latency() -> usize {
        BLOCK
    }

    pub fn blocks_processed(&self) -> u64 {
        self.blocks_processed
    }

    pub fn process(&mut self, left: &mut [f32], right: &mut [f32], params: Params) {
        let n = left.len().min(right.len());
        let mut i = 0;

        while i < n {
            let take = (n - i).min(BLOCK - self.fill);
            self.in_l[self.fill..self.fill + take].copy_from_slice(&left[i..i + take]);
            self.in_r[self.fill..self.fill + take].copy_from_slice(&right[i..i + take]);
            left[i..i + take].copy_from_slice(&self.delay_l[self.fill..self.fill + take]);
            right[i..i + take].copy_from_slice(&self.delay_r[self.fill..self.fill + take]);
            self.fill += take;
            i += take;

            if self.fill == BLOCK {
                if params.bypass == 0 {
                    self.apply_params(params);
                    let ctx = BlockContext {
                        tables: &self.tables,
                        meter_index: 0,
                        tempo: DUMMY_TEMPO_MILLI,
                        tick: 0,
                        stack_word: None,
                    };
                    self.left.process(&mut self.in_l, &ctx);
                    self.right.process(&mut self.in_r, &ctx);
                }
                std::mem::swap(&mut self.in_l, &mut self.delay_l);
                std::mem::swap(&mut self.in_r, &mut self.delay_r);
                self.fill = 0;
                self.blocks_processed += 1;
            }
        }
    }

    fn apply_params(&mut self, p: Params) {
        let amount = p.amount.min(127);
        let shape = p.shape.min(127);
        for inst in [&mut self.left, &mut self.right] {
            inst.set_param(dist::AMOUNT as u8, amount);
            inst.set_param(dist::SHAPE as u8, shape);
            inst.set_param(dist::ENABLED as u8, 1);
        }
    }
}

#[cfg(test)]
impl DistStereo {
    pub fn left(&self) -> &Distortion {
        &self.left
    }

    pub fn fill(&self) -> usize {
        self.fill
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn orb_dist_new() -> *mut DistStereo {
    Box::into_raw(Box::new(DistStereo::new()))
}

/// # Safety
/// `dist` is a pointer from [`orb_dist_new`], or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn orb_dist_free(dist: *mut DistStereo) {
    if !dist.is_null() {
        drop(unsafe { Box::from_raw(dist) });
    }
}

/// # Safety
/// `dist` is a pointer from [`orb_dist_new`], or null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn orb_dist_reset(dist: *mut DistStereo) {
    if let Some(p) = unsafe { dist.as_mut() } {
        p.reset();
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn orb_dist_latency() -> i32 {
    DistStereo::latency() as i32
}

/// # Safety
/// `dist` is a pointer from [`orb_dist_new`]. `left` and `right` each have `nframes` samples when
/// `nframes > 0`. `params` may be null, in which case defaults are used.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn orb_dist_process(
    dist: *mut DistStereo,
    left: *mut f32,
    right: *mut f32,
    nframes: i32,
    params: *const Params,
) {
    let Some(p) = (unsafe { dist.as_mut() }) else {
        return;
    };
    if nframes <= 0 || left.is_null() || right.is_null() {
        return;
    }
    let n = nframes as usize;
    let left = unsafe { std::slice::from_raw_parts_mut(left, n) };
    let right = unsafe { std::slice::from_raw_parts_mut(right, n) };
    let params = unsafe { params.as_ref() }.copied().unwrap_or_default();
    p.process(left, right, params);
}

#[cfg(test)]
mod tests;
