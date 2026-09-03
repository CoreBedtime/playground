//Created by Salty on 8/29/26.

#import <Cocoa/Cocoa.h>

@class PPMainViewController;

@interface PPListEditorController : NSViewController <NSTableViewDelegate, NSTableViewDataSource>
@property (weak) IBOutlet NSTableView *tableView;
@property (weak) IBOutlet NSButton *saveButton;

@property (weak) PPMainViewController *delegate;

- (instancetype)initWithDelegate:(PPMainViewController *)delegate;
- (void)presentWithInitialArray:(NSArray<NSString *> *)array;
@end
