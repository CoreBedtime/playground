//Created by Salty on 8/25/26.

#import <Cocoa/Cocoa.h>
#import <Swingset/Swingset.h>

#import "../Services/PPTweaksCollection.h"
#import "../Services/PPDaemonStatus.h"
#import "PPListEditorController.h"
#import "PPTweakEditorController.h"

@interface PPMainViewController : NSViewController <NSTableViewDelegate, NSTableViewDataSource>
@property (assign) BOOL disablePAC;
@property (assign) BOOL useLegacyAmmonia;
@property (assign) BOOL pauseInjection;
@property (strong) NSMutableArray<NSString *> *globalBlacklist;

- (void)updateGlobalBlacklistWithArray:(NSArray<NSString *> *)array;
//IB Outlets
@property (weak) IBOutlet NSTableView *tweaksTableView;
@property (weak) IBOutlet NSImageView *tweakStatusImageView;
@property (weak) IBOutlet NSTextField *tweakStatusLabel;
@property (weak) IBOutlet NSButton *installControlButton;
@end

@interface PPNSSwitch : NSSwitch
@property (strong) NSString *boundTweakMetafilePath;
@end

@interface PPNSButton : NSButton
@property (strong) NSString *boundTweakMetafilePath;

@end
